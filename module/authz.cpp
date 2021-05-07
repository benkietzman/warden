// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Authorization
// -------------------------------------
// file       : authz.cpp
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
#include <Storage>
#include <StringManip>
#include <Syslog>
#include <Warden>
using namespace common;
// }}}
// {{{ main()
int main(int argc, char *argv[])
{
  bool bProcessed = false;
  string strApplication = "Warden", strError, strJson, strUnix;
  stringstream ssMessage;
  Json *ptJson;
  Storage *pStorage = new Storage;
  StringManip manip;
  Syslog *pSyslog = NULL;

  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    string strArg = argv[i];
    if (strArg == "--syslog")
    {
      pSyslog = new Syslog(strApplication, "authz");
    }
    else if (strArg.size() > 7 && strArg.substr(0, 7) == "--unix=")
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
    string strSubError, strUser;
    stringstream ssCurrent;
    time_t CCurrent;
    Json *ptData;
    Warden warden(strApplication, strUnix, strError);
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
    // }}}
    if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty())
    {
      strUser = ptJson->m["User"]->v;
    }
    else if (ptJson->m.find("userid") != ptJson->m.end() && !ptJson->m["userid"]->v.empty())
    {
      strUser = ptJson->m["userid"]->v;
    }
    strSubError.clear();
    ptData = new Json(ptJson);
    if (warden.authn(ptData, strError))
    {
      Json *ptCentral = new Json;
      if (warden.central(strUser, ptCentral, strError))
      {
        bProcessed = true;
        ptJson->m["Data"] = new Json;
        ptJson->m["Data"]->insert("central", ptCentral);
        if (ptData->m.find("Data") != ptData->m.end())
        {
          ptJson->m["Data"]->insert("authn", ptData->m["Data"]);
        }
      }
      delete ptCentral;
    }
    delete ptData;
    if (pSyslog != NULL)
    {
      if (bProcessed)
      {
        pSyslog->logon("Authorized against Warden authz.", strUser);
      }
      else
      {
        pSyslog->logon("Failed to authorize against Warden authz.", strUser, false);
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
  cout << ptJson << endl;
  delete ptJson;
  delete pStorage;
  if (pSyslog != NULL)
  {
    delete pSyslog;
  }

  return 0;
}
// }}}
