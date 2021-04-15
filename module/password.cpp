// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Password
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
#include <fstream>
#include <iostream>
#include <list>
#include <mysql/mysql.h>
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
  string strError, strJson;
  stringstream ssMessage;
  Json *ptJson;
  Storage *pStorage = new Storage;
  StringManip manip;

  if (getline(cin, strJson))
  {
    string strSecret, strSubError;
    ptJson = new Json(strJson);
    // {{{ load cache
    if (ptJson->m.find("_storage") != ptJson->m.end())
    {
      pStorage->put(ptJson->m["_storage"]);
    }
    // }}}
    if (ptJson->m.find("Function") != ptJson->m.end() && !ptJson->m["Function"]->v.empty())
    {
      string strApplication, strFunction = ptJson->m["Function"]->v, strPassword, strType, strUser;
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
      // {{{ verify
      if (strFunction == "verify")
      {
        list<string> keys;
        Json *ptData = new Json;
        keys.push_back(strApplication);
        keys.push_back(strUser);
        if (pStorage->request("retrieve", keys, ptData, strError))
        {
          if ((strType.empty() || (ptData->m.find("Type") != ptData->m.end() && ptData->m["Type"]->v == strType)) && ptData->m.find("Password") != ptData->m.end() && ptData->m["Password"]->v == strPassword)
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
          Json *ptData = new Json;
          ptData->insert("Service", "password");
          ptData->insert("Function", "verify");
          ptData->insert("reqApp", "Warden");
          ptData->insert("reqMod", "password");
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
                ptData->insert("Type", strType);
                ptData->insert("Password", strPassword);
                if (pStorage->request("add", keys, ptData, strError))
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
        strError = "Please provide a valid Function:  verify.";
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
