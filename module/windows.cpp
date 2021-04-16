// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Windows
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
  bool bProcessed = false, bUpdated = false;
  string strDomain, strError, strJson;
  stringstream ssMessage;
  Json *ptJson;
  Storage *pStorage = new Storage;
  StringManip manip;

  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    string strArg = argv[i];
    if (strArg.size() > 9 && strArg.substr(0, 9) == "--domain=")
    {
      strDomain = strArg.substr(9, strArg.size() - 9);
      manip.purgeChar(strDomain, strDomain, "'");
      manip.purgeChar(strDomain, strDomain, "\"");
    }
  } 
  // }}}
  if (getline(cin, strJson))
  {
    string strSecret, strSubError;
    ptJson = new Json(strJson);
    // {{{ load cache
    if (ptJson->m.find("_storage") != ptJson->m.end())
    {
      pStorage->put(ptJson->m["_storage"]);
      delete ptJson->m["_storage"];
      ptJson->m.erase("_storage");
    }
    // }}}
    if (ptJson->m.find("Function") != ptJson->m.end() && !ptJson->m["Function"]->v.empty())
    {
      string strFunction = ptJson->m["Function"]->v, strPassword, strUser;
      if (ptJson->m.find("Password") != ptJson->m.end() && !ptJson->m["Password"]->v.empty())
      {
        strPassword = ptJson->m["Password"]->v;
      }
      if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty())
      {
        strUser = ptJson->m["User"]->v;
      }
      // {{{ login
      if (strFunction == "login")
      {
        list<string> keys;
        string strSubError;
        Json *ptData = new Json;
        keys.push_back(strUser);
        if (pStorage->retrieve(keys, ptData, strSubError))
        {
          if (ptData->v == strPassword)
          {
            bProcessed = true;
          }
          else
          {
            strError = "Failed authorization.";
          }
        }
        delete ptData;
        keys.clear();
        if (!bProcessed)
        {
          ServiceJunction junction(strError);
          list<string> in, out;
          ptData = new Json;
          junction.setApplication("Warden");
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
      // }}}
      // {{{ invalid
      else
      {
        strError = "Please provide a valid Function:  login.";
      }
      // }}}
    }
    else
    {
      strError = "Please provide the Function.";
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
