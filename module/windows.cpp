// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Windows Authentication
// -------------------------------------
// file       : windows.cpp
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
using namespace common;
// }}}
// {{{ main()
int main(int argc, char *argv[])
{
  bool bCached = false, bProcessed = false, bUpdated = false;
  string strConf, strDomain, strError, strJson;
  stringstream ssMessage;
  Json *ptJson;
  Storage *pStorage = new Storage;
  StringManip manip;

  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    string strArg = argv[i];
    if (strArg.size() > 7 && strArg.substr(0, 7) == "--conf=")
    {
      strConf = strArg.substr(7, strArg.size() - 7);
      manip.purgeChar(strConf, strConf, "'");
      manip.purgeChar(strConf, strConf, "\"");
    }
    else if (strArg.size() > 9 && strArg.substr(0, 9) == "--domain=")
    {
      strDomain = strArg.substr(9, strArg.size() - 9);
      manip.purgeChar(strDomain, strDomain, "'");
      manip.purgeChar(strDomain, strDomain, "\"");
    }
  } 
  // }}}
  if (getline(cin, strJson))
  {
    list<string> keys;
    string strPassword, strSubError, strUser;
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
        time_t CDuration = 3600, CModified;
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
        if (i->second->m.find("Status") == i->second->m.end() || i->second->m["Status"]->v != "okay")
        {
          CDuration = 300;
        }
        if ((CCurrent - CModified) > CDuration && pStorage->remove(keys, strSubError))
        {
          bUpdated = true;
        }
        keys.pop_back();
      }
    }
    delete ptData;
    keys.clear();
    // }}}
    if (ptJson->m.find("Password") != ptJson->m.end() && !ptJson->m["Password"]->v.empty())
    {
      strPassword = ptJson->m["Password"]->v;
    }
    if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty())
    {
      strUser = ptJson->m["User"]->v;
    }
    ptData = new Json;
    keys.push_back(strUser);
    if (pStorage->retrieve(keys, ptData, strSubError) && ptData->m.find("Password") != ptData->m.end() && ptData->m["Password"]->v == strPassword)
    {
      bCached = true;
      if (ptData->m.find("Status") != ptData->m.end() && ptData->m["Status"]->v == "okay")
      {
        bProcessed = true;
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
    if (!bCached)
    {
      ServiceJunction junction(strError);
      list<string> in, out;
      Json *ptStore = new Json;
      ptStore->insert("_modified", ssCurrent.str(), 'n');
      ptStore->insert("Password", strPassword);
      ptData = new Json;
      junction.setApplication("Warden");
      if (!strConf.empty())
      {
        junction.utility()->setConfPath(strConf, strError);
      }
      ptData->insert("Service", "samba");
      ptData->insert("Function", "login");
      ptData->insert("User", strUser);
      ptData->insert("Password", strPassword);
      ptData->insert("Domain", strDomain);
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
      keys.clear();
      delete ptStore;
    }
  }
  else
  {
    ptJson = new Json;
    strError = "Please provide the request.";
  }
  ptJson->insert("_cached", ((bCached)?"1":"0"), ((bCached)?'1':'0'));
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
