// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Central Authorization
// -------------------------------------
// file       : central.cpp
// author     : Ben Kietzman
// begin      : 2021-04-20
// copyright  : kietzman.org
// email      : ben@kietzman.org
///////////////////////////////////////////

/**************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
**************************************************************************/

// {{{ includes
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
using namespace std;
#include <Json>
#include <ServiceJunction>
#include <Storage>
#include <StringManip>
#include <Warden>
using namespace common;
// }}}
// {{{ main()
int main(int argc, char *argv[])
{
  bool bProcessed = false, bUpdated = false;
  string strError, strJson, strUnix;
  stringstream ssMessage;
  Json *ptJson;
  Storage *pStorage = new Storage;
  StringManip manip;

  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    string strArg = argv[i];
    if (strArg.size() > 7 && strArg.substr(0, 7) == "--unix=")
    {
      strUnix = strArg.substr(7, strArg.size() - 7);
      manip.purgeChar(strUnix, strUnix, "'");
      manip.purgeChar(strUnix, strUnix, "\"");
    }
  }
  // }}}
  if (getline(cin, strJson))
  {
    bool bDone = false;
    list<string> keys;
    string strApplication, strPassword, strSubError, strType, strUser;
    Json *ptData;
    stringstream ssCurrent;
    time_t CCurrent;
    time(&CCurrent);
    ssCurrent << CCurrent;
    ptJson = new Json(strJson);
    // {{{ load cache
    if (ptJson->m.find("_storage") != ptJson->m.end())
    {
      pStorage->put(ptJson->m["_storage"]);
      delete ptJson->m["_storage"];
      ptJson->m.erase("_storage");
    }
    ptData = new Json;
    if (pStorage->retrieve(keys, ptData, strSubError))
    {
      for (map<string, Json *>::iterator i = ptData->m.begin(); i != ptData->m.end(); i++)
      {
        stringstream ssModified;
        time_t CModified;
        keys.push_back(i->first);
        if (i->second->m.find("_modified") == i->second->m.end())
        {
          i->second->insert("_modified", ssCurrent.str(), 'n');
          if (pStorage->add(keys, i->second, strSubError))
          {
            bUpdated = true;
          }
        }
        ssModified.str(i->second->m["_modified"]->v);
        ssModified >> CModified;
        if ((CCurrent - CModified) > 300 && pStorage->remove(keys, strSubError))
        {
          bUpdated = true;
        }
        keys.pop_back();
      }
    }
    delete ptData;
    keys.clear();
    // }}}
    if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty())
    {
      strUser = ptJson->m["User"]->v;
    }
    ptData = new Json;
    keys.push_back(strUser);
    if (pStorage->retrieve(keys, ptData, strSubError))
    {
      bDone = true;
      if (ptData->m.find("Status") != ptData->m.end() && ptData->m["Status"]->v == "okay")
      {
        if (ptData->m.find("Data") != ptData->m.end())
        {
          bProcessed = true;
          ptJson->insert("Data", ptData->m["Data"]);
        }
        else
        {
          strError = "Failed to parse Data.";
        }
      }
      else if (ptData->m.find("Error") != ptData->m.end() && !ptData->m["Error"]->v.empty())
      {
        strError = ptData->m["Error"]->v;
      }
      else
      {
        strError = "Encountered an unknown error.";
      }
    }
    delete ptData;
    keys.clear();
    if (!bDone)
    {
      Json *ptConf = new Json, *ptStore = new Json;
      Warden warden("Central", strUnix, strError);
      ptStore->insert("_modified", ssCurrent.str(), 'n');
      keys.push_back("conf");
      if (warden.vaultRetrieve(keys, ptConf, strError))
      {
        if (ptConf->m.find("Database") != ptConf->m.end() && !ptConf->m["Database"]->v.empty())
        {
          if (ptConf->m.find("Database Password") != ptConf->m.end() && !ptConf->m["Database Password"]->v.empty())
          {
            if (ptConf->m.find("Database Server") != ptConf->m.end() && !ptConf->m["Database Server"]->v.empty())
            {
              if (ptConf->m.find("Database User") != ptConf->m.end() && !ptConf->m["Database User"]->v.empty())
              {
                list<string> in, out;
                string strValue;
                stringstream ssQuery;
                ServiceJunction junction(strError);
                StringManip manip;
                junction.setApplication("Warden");
                ptData = new Json;
                ptData->insert("Service", "mysql");
                ptData->insert("User", ptConf->m["Database User"]->v);
                ptData->insert("Password", ptConf->m["Database Password"]->v);
                ptData->insert("Server", ptConf->m["Database Server"]->v);
                ptData->insert("Database", ptConf->m["Database"]->v);
                ssQuery << "select id, active, admin, email, first_name, last_name, locked, userid from person where userid = '" << manip.escape(strUser, strValue) << "'";
                ptData->insert("Query", ssQuery.str());
                in.push_back(ptData->json(strJson));
                delete ptData;
                if (junction.request(in, out, strError))
                {
                  if (!out.empty())
                  {
                    Json *ptStatus = new Json(out.front());
                    if (ptStatus->m.find("Status") != ptStatus->m.end() && ptStatus->m["Status"]->v == "okay")
                    {
                      if (out.size() == 2)
                      {
                        map<string, string> getPersonRow;
                        Json *ptPerson = new Json(out.back());
                        ptPerson->flatten(getPersonRow, true, false);
                        delete ptPerson;
                        if (getPersonRow["active"] == "1")
                        {
                          if (getPersonRow["locked"] == "0")
                          {
                            list<string> subIn, subOut;
                            ptData = new Json;
                            ptData->insert("Service", "mysql");
                            ptData->insert("User", ptConf->m["Database User"]->v);
                            ptData->insert("Password", ptConf->m["Database Password"]->v);
                            ptData->insert("Server", ptConf->m["Database Server"]->v);
                            ptData->insert("Database", ptConf->m["Database"]->v);
                            ssQuery.str("");
                            ssQuery << "select a.admin, a.locked, b.id application_id, b.name application from application_contact a, application b where a.application_id = b.id and a.contact_id = '" << manip.escape(getPersonRow["id"], strValue) << "'";
                            ptData->insert("Query", ssQuery.str());
                            subIn.push_back(ptData->json(strJson));
                            delete ptData;
                            if (junction.request(subIn, subOut, strError))
                            {
                              if (!subOut.empty())
                              {
                                Json *ptSubStatus = new Json(subOut.front());
                                if (ptSubStatus->m.find("Status") != ptSubStatus->m.end() && ptSubStatus->m["Status"]->v == "okay")
                                {
                                  Json *ptApps = new Json;
                                  bProcessed = true;
                                  ptStore->m["Data"] = new Json(getPersonRow);
                                  ptStore->m["Data"]->insert("id", getPersonRow["id"], 'n');
                                  ptStore->m["Data"]->insert("active", getPersonRow["active"], ((getPersonRow["active"] == "1")?'1':'0'));
                                  ptStore->m["Data"]->insert("admin", getPersonRow["admin"], ((getPersonRow["admin"] == "1")?'1':'0'));
                                  subOut.pop_front();
                                  for (list<string>::iterator i = subOut.begin(); i != subOut.end(); i++)
                                  {
                                    map<string, string> getApplicationContactRow;
                                    Json *ptApplicationContact = new Json(*i);
                                    ptApplicationContact->flatten(getApplicationContactRow, true, false);
                                    delete ptApplicationContact;
                                    if (getApplicationContactRow["locked"] == "0" && (ptApps->m.find(getApplicationContactRow["application"]) == ptApps->m.end() || getApplicationContactRow["admin"] > ptApps->m[getApplicationContactRow["application"]]->v))
                                    {
                                      ptApps->insert(getApplicationContactRow["application"], getApplicationContactRow["admin"], ((getApplicationContactRow["admin"] == "1")?'1':'0'));
                                    }
                                    getApplicationContactRow.clear();
                                  }
                                  ptStore->m["Data"]->insert("apps", ptApps);
                                  ptJson->insert("Data", ptStore->m["Data"]);
                                  delete ptApps;
                                }
                                else if (ptSubStatus->m.find("Error") != ptSubStatus->m.end() && !ptSubStatus->m["Error"]->v.empty())
                                {
                                  strError = ptSubStatus->m["Error"]->v;
                                }
                                else
                                {
                                  strError = "Encountered an unknown error.";
                                }
                                delete ptSubStatus;
                              }
                              else
                              {
                                strError = "Failed to receive a response.";
                              }
                            }
                            subIn.clear();
                            subOut.clear();
                          }
                          else
                          {
                            strError = "Your account is locked.";
                          }
                        }
                        else
                        {
                          strError = "Your account is inactive.";
                        }
                        getPersonRow.clear();
                      }
                      else
                      {
                        strError = "User not found.";
                      }
                    }
                    else if (ptStatus->m.find("Error") != ptStatus->m.end() && !ptStatus->m["Error"]->v.empty())
                    {
                      strError = ptStatus->m["Error"]->v;
                    }
                    else
                    {
                      strError = "Encountered an unknown error.";
                    }
                    delete ptStatus;
                  }
                  else
                  {
                    strError = "Failed to receive a response.";
                  }
                }
                in.clear();
                out.clear();
              }
              else
              {
                strError = "Please provide the Database User.";
              }
            }
            else
            {
              strError = "Please provide the Database Server.";
            }
          }
          else
          {
            strError = "Please provide the Database Password.";
          }
        }
        else
        {
          strError = "Please provide the Database.";
        }
      }
      delete ptConf;
      keys.clear();
      ptStore->insert("Status", ((bProcessed)?"okay":"error"));
      if (!strError.empty())
      {
        ptStore->insert("Error", strError);
      }
      keys.push_back(strUser);
      if (pStorage->add(keys, ptStore, strSubError))
      {
        bUpdated = true;
      }
      delete ptStore;
      keys.clear();
    }
  }
  else
  {
    ptJson = new Json;
    strError = "Please provide the request.";
  }
  ptJson->insert("Status", ((bProcessed)?"okay":"error"));
  if (!strError.empty())
  {
    ptJson->insert("Error", strError);
  }
  if (bUpdated)
  {
    pStorage->lock();
    ptJson->insert("_storage", pStorage->ptr());
    pStorage->unlock();
  }
  cout << ptJson << endl;
  delete ptJson;
  delete pStorage;

  return 0;
}
// }}}
