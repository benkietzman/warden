// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Password Authentication
// -------------------------------------
// file       : password.cpp
// author     : Ben Kietzman
// begin      : 2021-04-15
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
    list<string> keys;
    string strApplication, strPassword, strSubError, strType, strUser;
    stringstream ssCurrent;
    time_t CCurrent;
    Json *ptData;
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
        if ((CCurrent - CModified) > 28800 && pStorage->remove(keys, strSubError))
        {
          bUpdated = true;
        }
        keys.pop_back();
      }
    }
    delete ptData;
    keys.clear();
    // }}}
    if (ptJson->m.find("Application") != ptJson->m.end() && !ptJson->m["Application"]->v.empty())
    {
      strApplication = ptJson->m["Application"]->v;
    }
    if (ptJson->m.find("Password") != ptJson->m.end() && !ptJson->m["Password"]->v.empty())
    {
      strPassword = ptJson->m["Password"]->v;
    }
    if (ptJson->m.find("Type") != ptJson->m.end() && !ptJson->m["Type"]->v.empty())
    {
      strType = ptJson->m["Type"]->v;
    }
    if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty())
    {
      strUser = ptJson->m["User"]->v;
    }
    if (!strApplication.empty())
    {
      string strSubError;
      ptData = new Json;
      keys.push_back(strApplication);
      keys.push_back(strUser);
      if (pStorage->retrieve(keys, ptData, strSubError) && (strType.empty() || (ptData->m.find("Type") != ptData->m.end() && ptData->m["Type"]->v == strType)) && ptData->m.find("Password") != ptData->m.end() && ptData->m["Password"]->v == strPassword)
      {
        bProcessed = true;
      }
      delete ptData;
      keys.clear();
      if (!bProcessed)
      {
        list<string> in, out;
        ServiceJunction junction(strError);
        junction.setApplication("Warden");
        ptData = new Json;
        ptData->insert("Service", "password");
        ptData->insert("Function", "verify");
        ptData->insert("Application", strApplication);
        ptData->insert("User", strUser);
        ptData->insert("Password", strPassword);
        ptData->insert("Type", strType);
        in.push_back(ptData->json(strJson));
        delete ptData;
        if (junction.request(in, out, strError))
        {
          if (!out.empty())
          {
            Json *ptStatus = new Json(out.front());
            if (ptStatus->m.find("Status") != ptStatus->m.end() && ptStatus->m["Status"]->v == "okay")
            {
              bProcessed = true;
              keys.push_back(strApplication);
              keys.push_back(strUser);
              ptData = new Json;
              ptData->insert("_modified", ssCurrent.str(), 'n');
              if (!strType.empty())
              {
                ptData->insert("Type", strType);
              }
              ptData->insert("Password", strPassword);
              if (pStorage->add(keys, ptData, strError))
              {
                bUpdated = true;
              }
              delete ptData;
              keys.clear();
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
    }
    else
    {
      ptData = new Json;
      keys.push_back(strUser);
      if (pStorage->retrieve(keys, ptData, strSubError) && ptData->m.find("Password") != ptData->m.end() && ptData->m["Password"]->v == strPassword)
      {
        bProcessed = true;
      }
      delete ptData;
      keys.clear();
      if (!bProcessed)
      {
        list<string> subKeys;
        Json *ptConf = new Json;
        Warden warden("Central", strUnix, strError);
        subKeys.push_back("conf");
        if (warden.vaultRetrieve(subKeys, ptConf, strError))
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
                  ssQuery << "select * from person where userid = '" << manip.escape(strUser, strValue) << "' and `password` = concat('*',upper(sha1(unhex(sha1('" << manip.escape(strPassword, strValue) << "')))))";
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
                        if (out.size() > 1)
                        {
                          bProcessed = true;
                          keys.push_back(strUser);
                          ptData = new Json;
                          ptData->insert("_modified", ssCurrent.str(), 'n');
                          ptData->insert("Password", strPassword);
                          if (pStorage->add(keys, ptData, strError))
                          {
                            bUpdated = true;
                          }
                          delete ptData;
                          keys.clear();
                        }
                        else
                        {
                          strError = "Failed authentication.";
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
        subKeys.clear();
      }
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
