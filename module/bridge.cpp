// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Bridge Authentication/Authorization
// -------------------------------------
// file       : bridge.cpp
// author     : Ben Kietzman
// begin      : 2021-05-10
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
#include <StringManip>
#include <Storage>
#include <Warden>
using namespace common;
// }}}
// {{{ main()
int main(int argc, char *argv[])
{
  bool bProcessed = false, bUpdated = false;
  string strApplication = "Warden", strError, strJson, strUnix;
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
        if ((CCurrent - CModified) > 3600 && pStorage->remove(keys, strSubError))
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
      Warden warden("Bridge", strUnix, strError);
      Json *ptStore = new Json;
      ptStore->insert("_modified", ssCurrent.str(), 'n');
      keys.push_back("bridge");
      keys.push_back(strUser);
      ptData = new Json;
      if (warden.vaultRetrieve(keys, ptData, strError))
      {
        if (ptData->m.find("Password") != ptData->m.end() && ptData->m["Password"]->v == strPassword)
        {
          bProcessed = true;
          ptStore->insert("Password", strPassword);
          ptStore->m["Data"] = new Json;
          if (ptData->m.find("Access") != ptData->m.end())
          {
            ptStore->m["Data"]->insert("Access", ptData->m["Access"]);
          }
          if (ptData->m.find("Application") != ptData->m.end() && !ptData->m["Application"]->v.empty())
          {
            ptStore->m["Data"]->insert("Application", ptData->m["Application"]->v);
          }
          if (ptData->m.find("Type") != ptData->m.end() && !ptData->m["Type"]->v.empty())
          {
            ptStore->m["Data"]->insert("Type", ptData->m["Type"]->v);
          }
          ptJson->insert("Data", ptStore->m["Data"]);
        }
        else
        {
          strError = "Access denied.";
        }
      }
      delete ptData;
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
