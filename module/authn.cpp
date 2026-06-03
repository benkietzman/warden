/* -*- c++ -*- */
///////////////////////////////////////////
// Authentication
// -------------------------------------
// file       : authn.cpp
// author     : Ben Kietzman
// begin      : 2021-04-16
// copyright  : Ben Kietzman
// email      : ben@kietzman.org
///////////////////////////////////////////
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
    string strErrorBridge, strErrorPasskey, strErrorPassword, strErrorRadial, strErrorWindows, strPassword, strSubError, strType, strUser;
    Json *ptPasskey = NULL;
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
    if (ptJson->m.find("passkey") != ptJson->m.end())
    {
      ptPasskey = new Json(ptJson->m["passkey"]);
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
      strError = (string)"[password-sla] " + strSubError;
    }
    else if (warden.bridge(strUser, strPassword, strErrorBridge) || warden.radial(strUser, strPassword, strErrorRadial) || warden.password(strUser, strPassword, strErrorPassword) || (ptPasskey != NULL && warden.passkey(ptPasskey, strErrorPasskey)) || warden.windows(strUser, strPassword, strErrorWindows))
    {
      bProcessed = true;
      if (ptPasskey != NULL)
      {
        ptJson->i("passkey-results", ptPasskey);
      }
      if (ptPasskey != NULL && ptPasskey->m.find("User") != ptPasskey->m.end() && !ptPasskey->m["User"]->v.empty())
      {
        ptJson->i("User", ptPasskey->m["User"]->v);
      }
    }
    else
    {
      stringstream ssError;
      ssError << "[bridge] " << strErrorBridge << " [passkey] " << strErrorPasskey << " [password] " << strErrorPassword << " [radial] " << strErrorRadial << " [windows] " << strErrorWindows;
      strError = ssError.str();
    }
    if (ptPasskey != NULL)
    {
      delete ptPasskey;
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

  return 0;
}
// }}}
