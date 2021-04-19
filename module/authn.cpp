// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Authentication
// -------------------------------------
// file       : authn.cpp
// author     : Ben Kietzman
// begin      : 2021-04-16
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
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
using namespace std;
#include <Json>
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
    bool bApplication = false;
    list<string> keys;
    string strApplication = "Warden", strPassword, strSecret, strSubError, strType, strUser;
    Json *ptData = new Json;
    Warden warden(strApplication, strUnix, strError);
    ptJson = new Json(strJson);
    // {{{ load cache
    if (ptJson->m.find("_storage") != ptJson->m.end())
    {
      pStorage->put(ptJson->m["_storage"]);
      delete ptJson->m["_storage"];
      ptJson->m.erase("_storage");
    }
    // }}}
    if (ptJson->m.find("Application") != ptJson->m.end() && !ptJson->m["Application"]->v.empty())
    {
      bApplication = true;
      strApplication = ptJson->m["Application"]->v;
    }
    if (ptJson->m.find("Password") != ptJson->m.end() && !ptJson->m["Password"]->v.empty())
    {
      strPassword = ptJson->m["Password"]->v;
    }
    else if (ptJson->m.find("password") != ptJson->m.end() && !ptJson->m["password"]->v.empty())
    {
      strPassword = ptJson->m["password"]->v;
    }
    if (ptJson->m.find("Type") != ptJson->m.end() && !ptJson->m["Type"]->v.empty())
    {
      strType = ptJson->m["Type"]->v;
    }
    if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty())
    {
      strUser = ptJson->m["User"]->v;
    }
    else if (ptJson->m.find("userid") != ptJson->m.end() && !ptJson->m["userid"]->v.empty())
    {
      strUser = ptJson->m["userid"]->v;
    }
    if (bApplication)
    {
      keys.push_back(strApplication);
      keys.push_back(strUser);
      if (pStorage->retrieve(keys, ptData, strSubError) && ptData->m.find("Password") == ptData->m.end() && ptData->v == strPassword && (strType.empty() || (ptData->m.find("Type") != ptData->m.end() && ptData->m["Type"]->v == strType)))
      {
        bProcessed = true;
      }
      delete ptData;
      keys.clear();
    }
    if (!bProcessed)
    {
      keys.push_back(strUser);
      if (pStorage->retrieve(keys, ptData, strSubError) && ptData->v == strPassword)
      {
        bProcessed = true;
      }
      delete ptData;
      keys.clear();
    }
    strSubError.clear();
    if (!bProcessed)
    {
      if (bApplication && warden.passwordVerify(strUser, strPassword, strType, strSubError))
      {
        bProcessed = true;
        keys.push_back(strApplication);
        keys.push_back(strUser);
        ptData = new Json;
        ptData->insert("Password", strPassword);
        if (!strType.empty())
        {
          ptData->insert("Type", strType);
        }
        if (pStorage->add(keys, ptData, strError))
        {
          bUpdated = true;
        }
        delete ptData;
        keys.clear();
      }
      else if (bApplication)
      {
        strError = strSubError;
      }
      else if (warden.passwordLogin(strUser, strPassword, strSubError) || warden.windowsLogin(strUser, strPassword, strSubError))
      {
        bProcessed = true;
        keys.push_back(strUser);
        ptData = new Json;
        ptData->value(strPassword);
        if (pStorage->add(keys, ptData, strError))
        {
          bUpdated = true;
        }
        delete ptData;
        keys.clear();
      }
      else
      {
        strError = strSubError;
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
