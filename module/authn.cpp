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
      pSyslog = new Syslog(strApplication, "authn");
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
    bool bApplication = false;
    string strPassword, strSubError, strType, strUser;
    Warden warden(strApplication, strUnix, strError);
    ptJson = new Json(strJson);
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
    if (bApplication && warden.password(strApplication, strUser, strPassword, strType, strSubError))
    {
      bProcessed = true;
    }
    else if (bApplication)
    {
      strError = strSubError;
    }
    else if (warden.bridge(strUser, strPassword, strSubError) || warden.radial(strUser, strPassword, strSubError) || warden.password(strUser, strPassword, strSubError) || warden.windows(strUser, strPassword, strSubError))
    {
      bProcessed = true;
    }
    else
    {
      strError = strSubError;
    }
    if (pSyslog != NULL)
    {
      if (bProcessed)
      {
        pSyslog->logon("Authenticated against Warden authn.", strUser);
      }
      else
      {
        pSyslog->logon("Failed to authenticate against Warden authn.", strUser, false);
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
