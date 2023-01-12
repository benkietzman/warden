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
    string strPassword, strUser;
    Json *ptData;
    Warden warden(strApplication, strUnix, strError);
    ptJson = new Json(strJson);
    if (ptJson->m.find("Password") != ptJson->m.end() && !ptJson->m["Password"]->v.empty())
    {
      strPassword = ptJson->m["Password"]->v;
    }
    else if (ptJson->m.find("password") != ptJson->m.end() && !ptJson->m["password"]->v.empty())
    {
      strPassword = ptJson->m["password"]->v;
    }
    if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty())
    {
      strUser = ptJson->m["User"]->v;
    }
    else if (ptJson->m.find("userid") != ptJson->m.end() && !ptJson->m["userid"]->v.empty())
    {
      strUser = ptJson->m["userid"]->v;
    }
    ptData = new Json(ptJson);
    if ((ptData->m.find("Password") == ptData->m.end() && ptData->m.find("password") == ptData->m.end()) || warden.authn(ptData, strError))
    {
      bool bFirst = true;
      string strSubError;
      stringstream ssSubError;
      Json *ptBridge = new Json, *ptCentral = new Json, *ptRadial = new Json;
      bProcessed = true;
      ptJson->m["Data"] = new Json;
      if (!ptData->m.empty())
      {
        ptJson->m["Data"]->insert("authn", ptData);
      }
      if (warden.bridge(strUser, strPassword, ptBridge, strSubError))
      {
        ptJson->m["Data"]->insert("bridge", ptBridge);
      }
      else
      {
        if (bFirst)
        {
          bFirst = false;
        }
        else
        {
          ssSubError << "  ";
        }
        ssSubError << "[bridge] " << strSubError;
      }
      delete ptBridge;
      if (warden.central(strUser, ptCentral, strSubError))
      {
        ptJson->m["Data"]->insert("central", ptCentral);
      }
      else
      {
        if (bFirst)
        {
          bFirst = false;
        }
        else
        {
          ssSubError << "  ";
        }
        ssSubError << "[central] " << strSubError;
      }
      delete ptCentral;
      if (warden.radial(strUser, strPassword, ptRadial, strSubError))
      {
        ptJson->m["Data"]->insert("radial", ptRadial);
      }
      else
      {
        if (bFirst)
        {
          bFirst = false;
        }
        else
        {
          ssSubError << "  ";
        }
        ssSubError << "[radial] " << strSubError;
      }
      delete ptRadial;
      if (!bFirst)
      {
        strError = ssSubError.str();
      }
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
  if (pSyslog != NULL)
  {
    delete pSyslog;
  }

  return 0;
}
// }}}
