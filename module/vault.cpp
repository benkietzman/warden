// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Vault
// -------------------------------------
// file       : vault.cpp
// author     : Ben Kietzman
// begin      : 2021-04-07
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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;
#include <File>
#include <Json>
#include <Storage>
#include <StringManip>
#include <Utility>
using namespace common;
// }}}
// {{{ main()
int main(int argc, char *argv[])
{
  bool bProcessed = false, bUpdated = false;
  string strAes, strConf, strData = "/data/password/vault", strError, strJson, strLock, strVault;
  stringstream ssMessage;
  File file;
  Json *ptJson;
  Storage *pStorage = new Storage;
  StringManip manip;
  Utility utility(strError);

  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    string strArg = argv[i];
    if (strArg == "-c" || (strArg.size() > 7 && strArg.substr(0, 7) == "--conf="))
    {
      if (strArg == "-c" && i + 1 < argc && argv[i+1][0] != '-')
      {
        strConf = argv[++i];
      }
      else
      {
        strConf = strArg.substr(7, strArg.size() - 7);
      }
      manip.purgeChar(strConf, strConf, "'");
      manip.purgeChar(strConf, strConf, "\"");
    }
    else if (strArg.size() > 7 && strArg.substr(0, 7) == "--data=")
    {
      strData = strArg.substr(7, strArg.size() - 7);
      manip.purgeChar(strData, strData, "'");
      manip.purgeChar(strData, strData, "\"");
    }
  }
  // }}}
  if (!strConf.empty())
  {
    strError.clear();
    utility.setConfPath(strConf, strError);
  }
  strAes = strData + "/.secret";
  strLock = strData + "/.lock";
  strVault = strData + "/storage";
  if (getline(cin, strJson))
  {
    bool bLoad = true;
    list<string> keys;
    string strSecret, strSubError;
    Json *ptData;
    ptJson = new Json(strJson);
    // {{{ load cache
    if (ptJson->m.find("_storage") != ptJson->m.end())
    {
      pStorage->put(ptJson->m["_storage"]);
      delete ptJson->m["_storage"];
      ptJson->m.erase("_storage");
    }
    // }}}
    // {{{ load secret
    ptData = new Json;
    keys.push_back("_secret");
    if (pStorage->retrieve(keys, ptData, strSubError) && !ptData->v.empty())
    {
      strSecret = ptData->v;
    }
    delete ptData;
    keys.clear();
    if (strSecret.empty())
    {
      ifstream inAes;
      inAes.open(strAes);
      if (inAes)
      {
        inAes >> strSecret;
      }
      else
      {
        ssMessage.str("");
        ssMessage << "ifstream::open(aes," << errno << ") " << strerror(errno);
        strError = ssMessage.str();
      }
      inAes.close();
    }
    // }}}
    // {{{ load modified
    ptData = new Json;
    keys.push_back("_modified");
    if (pStorage->retrieve(keys, ptData, strSubError))
    {
      stringstream ssModified(ptData->v);
      struct stat tStat;
      time_t CModified;
      ssModified >> CModified;
      if (stat(strVault.c_str(), &tStat) == 0 && tStat.st_mtime <= CModified)
      {
        bLoad = false;
      }
    }
    delete ptData;
    keys.clear();
    // }}}
    // {{{ load vault
    if (bLoad && !strSecret.empty())
    {
      struct stat tStat;
      if (stat(strVault.c_str(), &tStat) == 0)
      {
        ifstream inVault;
        inVault.open(strVault.c_str());
        if (inVault)
        {
          string strDecrypted;
          stringstream ssEncrypted;
          ssEncrypted << inVault.rdbuf();
          if (!manip.decryptAes(ssEncrypted.str(), strSecret, strDecrypted, strError).empty())
          {
            stringstream ssModified;
            Json *ptVault = new Json(strDecrypted);
            bUpdated = true;
            ssModified << tStat.st_mtime;
            ptVault->insert("_modified", ssModified.str(), 'n');
            ptVault->insert("_secret", strSecret);
            pStorage->put(ptVault);
            delete ptVault;
          }
          else
          {
            ifstream inAes;
            inAes.open(strAes);
            if (inAes)
            {
              inAes >> strSecret;
              if (!manip.decryptAes(ssEncrypted.str(), strSecret, strDecrypted, strError).empty())
              {
                stringstream ssModified;
                Json *ptVault = new Json(strDecrypted);
                bUpdated = true;
                ssModified << tStat.st_mtime;
                ptVault->insert("_modified", ssModified.str(), 'n');
                ptVault->insert("_secret", strSecret);
                pStorage->put(ptVault);
                delete ptVault;
              }
              else
              {
                ssMessage.str("");
                ssMessage << "StringManip::decryptAes() " << strError;
                strError = ssMessage.str();
              }
            }
            else
            {
              ssMessage.str("");
              ssMessage << "ifstream::open(aes," << errno << ") " << strerror(errno);
              strError = ssMessage.str();
            }
            inAes.close();
          }
        }
        else
        {
          ssMessage.str("");
          ssMessage << "ifstream::open(vault," << errno << ") " << strerror(errno);
          strError = ssMessage.str();
        }
        inVault.close();
      }
    }
    // }}}
    if (ptJson->m.find("Function") != ptJson->m.end() && !ptJson->m["Function"]->v.empty())
    {
      string strFunction = ptJson->m["Function"]->v;
      if (ptJson->m.find("Keys") != ptJson->m.end())
      {
        for (list<Json *>::iterator i = ptJson->m["Keys"]->l.begin(); i != ptJson->m["Keys"]->l.end(); i++)
        {
          if (!(*i)->v.empty())
          {
            keys.push_back((*i)->v);
          }
        }
      }
      // {{{ add | remove | update
      if (strFunction == "add" || strFunction == "remove" || strFunction == "update")
      {
        ptData = NULL;
        if (ptJson->m.find("Data") != ptJson->m.end())
        {
          ptData = new Json(ptJson->m["Data"]);
        }
        if (pStorage->request(strFunction, keys, ptData, strError))
        {
          bProcessed = true;
          if (!strSecret.empty())
          {
            string strEncrypted, strDecrypted;
            Json *ptVault = pStorage->get();
            if (ptVault->m.find("_modified") != ptVault->m.end())
            {
              delete ptVault->m["_modified"];
              ptVault->m.erase("_modified");
            }
            if (ptVault->m.find("_secret") != ptVault->m.end())
            {
              delete ptVault->m["_secret"];
              ptVault->m.erase("_secret");
            }
            ptVault->json(strDecrypted);
            delete ptVault;
            if (!manip.encryptAes(strDecrypted, strSecret, strEncrypted, strError).empty())
            {
              ofstream outLock, outVault;
              while (file.fileExist(strLock))
              {
                utility.msleep(250);
              }
              outLock.open(strLock.c_str());
              outLock.close();
              outVault.open(strVault);
              if (outVault)
              {
                bUpdated = true;
                outVault << strEncrypted;
              }
              else
              {
                ssMessage.str("");
                ssMessage << "ofstream::open(vault," << errno << ") " << strerror(errno);
                strError = ssMessage.str();
              }
              outVault.close();
              file.remove(strLock);
            }
          }
        }
        if (ptData != NULL)
        {
          delete ptData;
        }
      }
      // }}}
      // {{{ retrieve | retrieveKeys
      else if (strFunction == "retrieve" || strFunction == "retrieveKeys")
      {
        ptData = new Json;
        if (pStorage->request(strFunction, keys, ptData, strError))
        {
          bProcessed = true;
          if (keys.empty())
          {
            if (ptData->m.find("_modified") != ptData->m.end())
            {
              delete ptData->m["_modified"];
              ptData->m.erase("_modified");
            }
            if (ptData->m.find("_secret") != ptData->m.end())
            {
              delete ptData->m["_secret"];
              ptData->m.erase("_secret");
            }
          }
          ptJson->insert("Data", ptData);
        }
        delete ptData;
      }
      // }}}
      // {{{ invalid
      else
      {
        strError = "Please provide a valid Function:  add, remove, retrieve, retrieveKeys, update.";
      }
      // }}}
      keys.clear();
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
