// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Warden
// -------------------------------------
// file       : warden.cpp
// author     : Ben Kietzman
// begin      : 2021-04-06
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

/*! \file warden.cpp
* \brief Warden Daemon
*
* Provides socket-level access to modules.
*/
// {{{ includes
#include <cerrno>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
using namespace std;
#include <Central>
#include <Json>
#include <SignalHandling>
#include <Storage>
using namespace common;
// }}}
// {{{ defines
#ifdef VERSION
#undef VERSION
#endif
/*! \def VERSION
* \brief Contains the application version number.
*/
#define VERSION "0.1"
/*! \def mUSAGE(A)
* \brief Prints the usage statement.
*/
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " --conf=[CONF]" << endl << "     Provides the configuration path." << endl << endl << " -d, --daemon" << endl << "     Turns the process into a daemon." << endl << endl << "     --data" << endl << "     Sets the data directory." << endl << endl << " -e EMAIL, --email=EMAIL" << endl << "     Provides the email address for default notifications." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << "           --max-buffer=[MAX BUFFER]" << endl << "     Provides the maximum input buffer limit in MBs." << endl << endl << "           --max-lines=[MAX LINES]" << endl << "     Provides the maximum input lines limit." << endl << endl << " -v, --version" << endl << "     Displays the current version of this software." << endl << endl
/*! \def mVER_USAGE(A,B)
* \brief Prints the version number.
*/
#define mVER_USAGE(A,B) cout << endl << A << " Version: " << B << endl << endl
/*! \def CHILD_TIMEOUT
* \brief Supplies the child timeout.
*/
#define CHILD_TIMEOUT 3600
/*! \def MODULE_CONFIG
* \brief Contains the module configuration path.
*/
#define MODULE_CONFIG "/modules.conf"
/*! \def PID
* \brief Contains the PID path.
*/
#define PID "/.pid"
/*! \def START
* \brief Contains the start path.
*/
#define START "/.start"
/*! \def STORAGE_SOCKET
* \brief Contains the unix socket path.
*/
#define STORAGE_SOCKET "/.storage"
/*! \def UNIX_SOCKET
* \brief Contains the unix socket path.
*/
#define UNIX_SOCKET "/socket"
#define PARENT_READ  readpipe[0]
#define CHILD_WRITE  readpipe[1]
#define CHILD_READ   writepipe[0]
#define PARENT_WRITE writepipe[1]
// }}}
// {{{ structs
struct connection
{
  int readpipe;
  int writepipe;
  pid_t childPid;
  string strBuffer[2];
  string strCommand;
  time_t CStartTime;
  time_t CEndTime;
  time_t CTimeout;
  timespec start;
  timespec stop[2];
  Json *ptRequest;
};
// }}}
// {{{ global variables
extern char **environ;
static bool gbDaemon = false; //!< Global daemon variable.
static bool gbShutdown = false; //!< Global shutdown variable.
static string gstrApplication = "Warden"; //!< Global application name.
static string gstrData = "/data/warden"; //!< Global data path.
static string gstrEmail; //!< Global notification email address.
static Central *gpCentral = NULL; //!< Contains the Central class.
// }}}
// {{{ prototypes
/*! \fn void sighandle(const int nSignal)
* \brief Establishes signal handling for the application.
* \param nSignal Contains the caught signal.
*/
void sighandle(const int nSignal);
/*! \fn bool storage(const string strAction, list<string> keys, Json *ptData, string &strError)
* \brief Interfaces with storage.
* \param strAction Contains the action.
* \param keys Contains the keys.
* \param ptData Contains the data.
* \param strError Returns an error.
*/
bool storage(const string strAction, list<string> keys, Json *ptData, string &strError);
// }}}
// {{{ main()
/*! \fn int main(int argc, char *argv[])
* \brief This is the main function.
* \return Exits with a return code for the operating system.
*/
int main(int argc, char *argv[])
{
  string strConf, strError, strPrefix = "main()";
  stringstream ssMessage;

  gpCentral = new Central(strError);
  // {{{ set signal handling
  sethandles(sighandle);
  signal(SIGBUS, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGCONT, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGSEGV, SIG_IGN);
  signal(SIGWINCH, SIG_IGN);
  // }}}
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
    else if (strArg == "-d" || strArg == "--daemon")
    {
      gbDaemon = true;
    }
    else if (strArg.size() > 7 && strArg.substr(0, 7) == "--data=")
    {
      gstrData = strArg.substr(7, strArg.size() - 7);
      gpCentral->manip()->purgeChar(gstrData, gstrData, "'");
      gpCentral->manip()->purgeChar(gstrData, gstrData, "\"");
    }
    else if (strArg == "-e" || (strArg.size() > 8 && strArg.substr(0, 8) == "--email="))
    {
      if (strArg == "-e" && i + 1 < argc && argv[i+1][0] != '-')
      {
        gstrEmail = argv[++i];
      }
      else
      {
        gstrEmail = strArg.substr(8, strArg.size() - 8);
      }
      gpCentral->manip()->purgeChar(gstrEmail, gstrEmail, "'");
      gpCentral->manip()->purgeChar(gstrEmail, gstrEmail, "\"");
    }
    else if (strArg == "-h" || strArg == "--help")
    {
      mUSAGE(argv[0]);
      return 0;
    }
    else if (strArg == "-v" || strArg == "--version")
    {
      mVER_USAGE(argv[0], VERSION);
      return 0;
    }
    else
    {
      cout << endl << "Illegal option, '" << strArg << "'." << endl;
      mUSAGE(argv[0]);
      return 0;
    }
  }
  // }}}
  if (!strConf.empty())
  {
    gpCentral->junction()->setConfPath(strConf, strError);
    gpCentral->radial()->setConfPath(strConf, strError);
    gpCentral->utility()->setConfPath(strConf, strError);
  }
  gpCentral->setApplication(gstrApplication);
  gpCentral->setEmail(gstrEmail);
  gpCentral->setLog(gstrData, "warden_", "daily", true, true);
  gpCentral->setRoom("#system");
  // {{{ normal run
  if (!gstrEmail.empty())
  {
    if (!gbShutdown)
    {
      pid_t nStoragePid;
      if (gbDaemon)
      {
        gpCentral->utility()->daemonize();
      }
      setlocale(LC_ALL, "");
      ofstream outPid((gstrData + PID).c_str());
      if (outPid.good())
      {
        outPid << getpid() << endl;
      }
      outPid.close();
      ofstream outStart((gstrData + START).c_str());
      outStart.close();
      // {{{ process
      if ((nStoragePid = fork()) > 0)
      {
        int fdUnix = -1, nReturn;
        gpCentral->file()->remove((gstrData + UNIX_SOCKET).c_str());
        if ((fdUnix = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0)
        {
          sockaddr_un addr;
          ssMessage.str("");
          ssMessage << strPrefix << "->socket():  Created the socket.";
          gpCentral->log(ssMessage.str());
          memset(&addr, 0, sizeof(sockaddr_un));
          addr.sun_family = AF_UNIX;
          strncpy(addr.sun_path, (gstrData + UNIX_SOCKET).c_str(), sizeof(addr.sun_path) - 1);
          if (bind(fdUnix, (sockaddr *)&addr, sizeof(sockaddr_un)) == 0)
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->bind():  Bound to the socket.";
            gpCentral->log(ssMessage.str());
            chmod((gstrData + UNIX_SOCKET).c_str(), 00770);
            if (listen(fdUnix, SOMAXCONN) == 0)
            {
              int fdData;
              string strSystem;
              sockaddr_un cli_addr;
              socklen_t clilen = sizeof(cli_addr);
              time_t ulModifyTime = 0;
              map<string, map<string, string> > module;
              ssMessage.str("");
              ssMessage << strPrefix << "->listen():  Listening to the socket.";
              gpCentral->log(ssMessage.str());
              clilen = sizeof(cli_addr);
              while (!gbShutdown && (fdData = accept(fdUnix, (struct sockaddr *)&cli_addr, &clilen)) >= 0)
              {
                // {{{ reload module configuration
                if (!gpCentral->file()->fileExist(gstrData + (string)"/.lock"))
                {
                  struct stat tStat;
                  if (stat((gstrData + MODULE_CONFIG).c_str(), &tStat) == 0)
                  {
                    if (ulModifyTime != tStat.st_mtime)
                    {
                      ifstream inFile;
                      ssMessage.str("");
                      ssMessage << strPrefix << ":  " << ((ulModifyTime == 0)?"L":"Rel") << "oaded the configuration file.";
                      gpCentral->log(ssMessage.str());
                      inFile.open((gstrData + MODULE_CONFIG).c_str());
                      if (inFile.good())
                      {
                        string strConf;
                        ulModifyTime = tStat.st_mtime;
                        module.clear();
                        while (getline(inFile, strConf).good())
                        {
                          map<string, string> moduleMap;
                          Json *ptJson = new Json(strConf);
                          ptJson->flatten(moduleMap, true, false);
                          delete ptJson;
                          if (!moduleMap.empty())
                          {
                            if (moduleMap.find("Module") != moduleMap.end() && !moduleMap["Module"].empty())
                            {
                              if (moduleMap.find("Command") != moduleMap.end() && !moduleMap["Command"].empty())
                              {
                                module[moduleMap["Module"]] = moduleMap;
                              }
                              else
                              {
                                ssMessage.str("");
                                ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Invalid module configuration.  Please provide the Command. --- " << strConf;
                                gpCentral->notify(ssMessage.str());
                              }
                            }
                            else
                            {
                              ssMessage.str("");
                              ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Invalid module configuration.  Please provide the Module. --- " << strConf;
                              gpCentral->notify(ssMessage.str());
                            }
                          }
                          else
                          {
                            ssMessage.str("");
                            ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Invalid JSON formatting in module configuration. --- " << strConf;
                            gpCentral->notify(ssMessage.str());
                          }
                          moduleMap.clear();
                        }
                      }
                      else
                      {
                        ssMessage.str("");
                        ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Unable to open module configuration for reading.";
                        gpCentral->alert(ssMessage.str());
                      }
                      inFile.close();
                    }
                  }
                  else
                  {
                    ssMessage.str("");
                    ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Unable to locate module configuration.";
                    gpCentral->alert(ssMessage.str());
                  }
                }
                // }}}
                if (fork() == 0)
                {
                  bool bExit = false;
                  char szBuffer[65536];
                  list<connection *> queue;
                  pollfd *fds;
                  size_t unPosition, unfdSize;
                  string strBuffer[2];
                  while (!bExit)
                  {
                    fds = new pollfd[(queue.size() * 2) + 1];
                    unfdSize = 0;
                    fds[unfdSize].fd = fdData;
                    fds[unfdSize].events = POLLIN;
                    if (!strBuffer[1].empty())
                    {
                      fds[unfdSize].events |= POLLOUT;
                    }
                    unfdSize++;
                    for (list<connection *>::iterator i = queue.begin(); i != queue.end(); i++)
                    {
                      fds[unfdSize].fd = (*i)->readpipe;
                      fds[unfdSize].events = POLLIN;
                      unfdSize++;
                      fds[unfdSize].fd = -1;
                      if (!(*i)->strBuffer[1].empty())
                      {
                        fds[unfdSize].fd = (*i)->writepipe;
                        fds[unfdSize].events = POLLOUT;
                      }
                      unfdSize++;
                    }
                    if ((nReturn = poll(fds, unfdSize, 250)) > 0)
                    {
                      for (size_t unfdIndex = 0; unfdIndex < unfdSize; unfdIndex++)
                      {
                        list<list<connection *>::iterator> removeList;
                        // {{{ client socket
                        if (fds[unfdIndex].fd == fdData)
                        {
                          // {{{ read
                          if (fds[unfdIndex].revents & POLLIN)
                          {
                            if ((nReturn = read(fdData, szBuffer, 65536)) > 0)
                            {
                              strBuffer[0].append(szBuffer, nReturn);
                              while ((unPosition = strBuffer[0].find("\n")) != string::npos)
                              {
                                Json *ptRequest = new Json(strBuffer[0].substr(0, unPosition));
                                strBuffer[0].erase(0, (unPosition + 1));
                                if (ptRequest->m.find("Module") != ptRequest->m.end() && !ptRequest->m["Module"]->v.empty())
                                {
                                  string strCommand;
                                  if (module.find(ptRequest->m["Module"]->v) != module.end())
                                  {
                                    strCommand = module[ptRequest->m["Module"]->v]["Command"];
                                  }
                                  if (!strCommand.empty())
                                  {
                                    char *args[100], *pszArgument;
                                    int readpipe[2] = {-1, -1}, writepipe[2] = {-1, -1};
                                    pid_t childPid;
                                    string strArgument;
                                    stringstream ssCommand;
                                    time_t CStartTime = 0, CEndTime = 0, CTimeout = CHILD_TIMEOUT;
                                    unsigned int unIndex = 0;
                                    time(&CStartTime);
                                    if (ptRequest->m.find("Timeout") != ptRequest->m.end() && !ptRequest->m["Timeout"]->v.empty())
                                    {
                                      bool bNumeric = true;
                                      for (unsigned int i = 0; i < ptRequest->m["Timeout"]->v.size(); i++)
                                      {
                                        if (!isdigit(ptRequest->m["Timeout"]->v[i]))
                                        {
                                          bNumeric = false;
                                        }
                                      }
                                      if (bNumeric)
                                      {
                                        CTimeout = atoi(ptRequest->m["Timeout"]->v.c_str());
                                      }
                                    }
                                    ssCommand.str(strCommand);
                                    while (ssCommand >> strArgument)
                                    {
                                      pszArgument = new char[strArgument.size() + 1];
                                      strcpy(pszArgument, strArgument.c_str());
                                      args[unIndex++] = pszArgument;
                                    }
                                    args[unIndex] = NULL;
                                    if (pipe(readpipe) == 0)
                                    {
                                      if (pipe(writepipe) == 0)
                                      {
                                        if ((childPid = fork()) == 0)
                                        {
                                          close(PARENT_WRITE);
                                          close(PARENT_READ);
                                          dup2(CHILD_READ, 0);
                                          close(CHILD_READ);
                                          dup2(CHILD_WRITE, 1);
                                          close(CHILD_WRITE);
                                          execve(args[0], args, environ);
                                          _exit(1);
                                        }
                                        else if (childPid > 0)
                                        {
                                          list<string> keys;
                                          string strJson, strSubError;
                                          Json *ptData = new Json;
                                          connection *ptConnection = new connection;
                                          close(CHILD_READ);
                                          close(CHILD_WRITE);
                                          ptConnection->readpipe = PARENT_READ;
                                          ptConnection->writepipe = PARENT_WRITE;
                                          ptConnection->ptRequest = new Json(ptRequest);
                                          keys.push_back("modules");
                                          keys.push_back(ptRequest->m["Module"]->v);
                                          clock_gettime(CLOCK_REALTIME, &(ptConnection->start));
                                          if (storage("retrieve", keys, ptData, strSubError))
                                          {
                                            ptConnection->ptRequest->insert("_storage", ptData);
                                          }
                                          else if (strSubError != "Failed to find key.")
                                          {
                                            ssMessage.str("");
                                            ssMessage << strPrefix << "->storage() error:  " << strSubError;
                                            gpCentral->log(ssMessage.str());
                                          }
                                          clock_gettime(CLOCK_REALTIME, &(ptConnection->stop[0]));
                                          keys.clear();
                                          delete ptData;
                                          ptConnection->childPid = childPid;
                                          ptConnection->CStartTime = CStartTime;
                                          ptConnection->CEndTime = CEndTime;
                                          ptConnection->CTimeout = CTimeout;
                                          ptConnection->strBuffer[1] = ptConnection->ptRequest->json(strJson) + "\n";
                                          ptConnection->strCommand = strCommand;
                                          queue.push_back(ptConnection);
                                        }
                                        else
                                        {
                                          ssMessage.str("");
                                          ssMessage << "fork(" << errno << ") " << strerror(errno);
                                          strError = ssMessage.str();
                                        }
                                      }
                                      else
                                      {
                                        ssMessage.str("");
                                        ssMessage << "pipe(write," << errno << ") " << strerror(errno);
                                        strError = ssMessage.str();
                                      }
                                    }
                                    else
                                    {
                                      ssMessage.str("");
                                      ssMessage << "pipe(read," << errno << ") " << strerror(errno);
                                      strError = ssMessage.str();
                                    }
                                    for (unsigned int i = 0; i < unIndex; i++)
                                    {
                                      delete[] args[i];
                                    }
                                  }
                                  else
                                  {
                                    strError = "The requested module does not exist.";
                                  }
                                }
                                else
                                {
                                  strError = "Please provide the Module.";
                                }
                                if (!strError.empty())
                                {
                                  string strJson;
                                  ptRequest->insert("Status", "error");
                                  ptRequest->insert("Error", strError);
                                  ptRequest->json(strJson);
                                  strJson += "\n";
                                  strBuffer[1].append(strJson);
                                }
                                delete ptRequest;
                              }
                            }
                            else
                            {
                              bExit = true;
                            }
                          }
                          // }}}
                          // {{{ write
                          if (fds[unfdIndex].revents & POLLOUT)
                          {
                            if ((nReturn = write(fdData, strBuffer[1].c_str(), strBuffer[1].size())) > 0)
                            {
                              strBuffer[1].erase(0, nReturn);
                            }
                            else
                            {
                              bExit = true;
                            }
                          }
                          // }}}
                        }
                        // }}}
                        // {{{ module pipes
                        for (list<connection *>::iterator i = queue.begin(); i != queue.end(); i++)
                        {
                          bool bDone = false;
                          string strError;
                          // {{{ read
                          if (fds[unfdIndex].fd == (*i)->readpipe && (fds[unfdIndex].revents & (POLLHUP | POLLIN)))
                          {
                            char szBuffer[65536];
                            ssize_t nSubReturn;
                            if ((nSubReturn = read((*i)->readpipe, szBuffer, 65536)) > 0)
                            {
                              (*i)->strBuffer[0].append(szBuffer, nSubReturn);
                            }
                            else
                            {
                              bDone = true;
                              if (nSubReturn < 0)
                              {
                                stringstream ssError;
                                ssError << "read(" << errno << ") " << strerror(errno);
                                strError = ssError.str();
                              }
                            }
                          }
                          // }}}
                          // {{{ write
                          if (fds[unfdIndex].fd == (*i)->writepipe && (fds[unfdIndex].revents & (POLLHUP | POLLOUT)))
                          {
                            ssize_t nSubReturn;
                            if ((nSubReturn = write((*i)->writepipe, (*i)->strBuffer[1].c_str(), (*i)->strBuffer[1].size())) > 0)
                            {
                              (*i)->strBuffer[1].erase(0, nSubReturn);
                            }
                            else
                            {
                              bDone = true;
                              if (nSubReturn < 0)
                              {
                                stringstream ssError;
                                ssError << "write(" << errno << ") " << strerror(errno);
                                strError = ssError.str();
                              }
                            }
                          }
                          // }}}
                          time(&((*i)->CEndTime));
                          if (((*i)->CEndTime - (*i)->CStartTime) > (*i)->CTimeout)
                          {
                            bDone = true;
                            strError = "Request timed out.";
                            kill((*i)->childPid, SIGTERM);
                          }
                          // {{{ done
                          if (bDone)
                          {
                            size_t unDuration[2];
                            string strJson;
                            stringstream ssDuration[2];
                            Json *ptResponse = new Json((*i)->strBuffer[0]);
                            close((*i)->readpipe);
                            close((*i)->writepipe);
                            if (ptResponse->m.find("_storage") != ptResponse->m.end())
                            {
                              if ((*i)->ptRequest->m.find("Module") != (*i)->ptRequest->m.end() && !(*i)->ptRequest->m["Module"]->v.empty())
                              {
                                list<string> keys;
                                string strSubError;
                                keys.push_back("modules");
                                keys.push_back((*i)->ptRequest->m["Module"]->v);
                                if (!storage("add", keys, ptResponse->m["_storage"], strSubError))
                                {
                                  ssMessage.str("");
                                  ssMessage << strPrefix << "->storage() error:  " << strSubError;
                                  gpCentral->log(ssMessage.str());
                                }
                                keys.clear();
                              }
                              delete ptResponse->m["_storage"];
                              ptResponse->m.erase("_storage");
                            }
                            if ((*i)->ptRequest->m.find("Module") != (*i)->ptRequest->m.end() && !(*i)->ptRequest->m["Module"]->v.empty())
                            {
                              clock_gettime(CLOCK_REALTIME, &((*i)->stop[1]));
                              unDuration[0] = (((*i)->stop[0].tv_sec - (*i)->start.tv_sec) * 1000) + (((*i)->stop[0].tv_nsec - (*i)->start.tv_nsec) / 1000000);
                              ssDuration[0] << unDuration[0];
                              unDuration[1] = (((*i)->stop[1].tv_sec - (*i)->start.tv_sec) * 1000) + (((*i)->stop[1].tv_nsec - (*i)->start.tv_nsec) / 1000000);
                              ssDuration[1] << unDuration[1];
                              if (ptResponse->m.find("_duration") == ptResponse->m.end())
                              {
                                ptResponse->m["_duration"] = new Json;
                              }
                              if (ptResponse->m["_duration"]->m.find((*i)->ptRequest->m["Module"]->v) == ptResponse->m["_duration"]->m.end())
                              {
                                ptResponse->m["_duration"]->m[(*i)->ptRequest->m["Module"]->v] = new Json;
                              }
                              ptResponse->m["_duration"]->m[(*i)->ptRequest->m["Module"]->v]->insert("storage", ssDuration[0].str(), 'n');
                              ptResponse->m["_duration"]->m[(*i)->ptRequest->m["Module"]->v]->insert("request", ssDuration[1].str(), 'n');
                            }
                            strBuffer[1].append(ptResponse->json(strJson)+"\n");
                            delete ptResponse;
                            (*i)->strBuffer[0].clear();
                            delete (*i)->ptRequest;
                            delete *i;
                            removeList.push_back(i);
                          }
                          // }}}
                        }
                        // }}}
                        for (list<list<connection *>::iterator>::iterator i = removeList.begin(); i != removeList.end(); i++)
                        {
                          queue.erase(*i);
                        }
                        removeList.clear();
                      }
                    }
                    else if (nReturn < 0)
                    {
                      bExit = true;
                      ssMessage.str("");
                      ssMessage << strPrefix << "->poll(" << errno << ") error:  " << strerror(errno);
                      gpCentral->log(ssMessage.str());
                    }
                    delete[] fds;
                  }
                  for (list<connection *>::iterator i = queue.begin(); i != queue.end(); i++)
                  {
                    close((*i)->readpipe);
                    close((*i)->writepipe);
                    (*i)->strBuffer[0].clear();
                    (*i)->strBuffer[1].clear();
                    delete (*i)->ptRequest;
                    delete *i;
                    ssMessage.str("");
                    ssMessage << strPrefix << ":  Removed unaccounted for request from queue.";
                    gpCentral->log(ssMessage.str());
                  }
                  queue.clear();
                  close(fdData);
                  _exit(1);
                }
                else
                {
                  close(fdData);
                }
              }
              if (!gbShutdown)
              {
                gbShutdown = true;
              }
            }
            else
            {
              ssMessage.str("");
              ssMessage << strPrefix << "->listen(" << errno << ") error:  " << strerror(errno);
              gpCentral->alert(ssMessage.str());
            }
            close(fdUnix);
            ssMessage.str("");
            ssMessage << strPrefix << ":  Closed the socket.";
            gpCentral->log(ssMessage.str());
          }
          else
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->bind(" << errno << ") error:  " << strerror(errno);
            gpCentral->alert(ssMessage.str());
          }
        }
        else
        {
          ssMessage.str("");
          ssMessage << strPrefix << "->socket(" << errno << ") error:  " << strerror(errno);
          gpCentral->alert(ssMessage.str());
        }
        gpCentral->file()->remove((gstrData + UNIX_SOCKET).c_str());
        kill(nStoragePid, SIGTERM);
      }
      // }}}
      // {{{ storage
      else if (nStoragePid == 0)
      {
        int fdUnix = -1, nReturn;
        gpCentral->file()->remove((gstrData + STORAGE_SOCKET).c_str());
        if ((fdUnix = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0)
        {
          sockaddr_un addr;
          ssMessage.str("");
          ssMessage << strPrefix << "->socket():  Created the storage socket.";
          gpCentral->log(ssMessage.str());
          memset(&addr, 0, sizeof(sockaddr_un));
          addr.sun_family = AF_UNIX;
          strncpy(addr.sun_path, (gstrData + STORAGE_SOCKET).c_str(), sizeof(addr.sun_path) - 1);
          if (bind(fdUnix, (sockaddr *)&addr, sizeof(sockaddr_un)) == 0)
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->bind():  Bound to the storage socket.";
            gpCentral->log(ssMessage.str());
            chmod((gstrData + STORAGE_SOCKET).c_str(), 00700);
            if (listen(fdUnix, SOMAXCONN) == 0)
            {
              bool bExit = false;
              char szBuffer[65536];
              map<int, vector<string> > clients;
              list<int> removals;
              pollfd *fds;
              size_t unIndex, unPosition;
              string strJson;
              Storage *pStorage = new Storage;
              ssMessage.str("");
              ssMessage << strPrefix << "->listen():  Listening to the storage socket.";
              gpCentral->log(ssMessage.str());
              while (!gbShutdown && !bExit)
              {
                fds = new pollfd[clients.size()+1];
                unIndex = 0;
                fds[unIndex].fd = fdUnix;
                fds[unIndex].events = POLLIN;
                unIndex++;
                for (map<int, vector<string> >::iterator i = clients.begin(); i != clients.end(); i++)
                {
                  fds[unIndex].fd = i->first;
                  fds[unIndex].events = POLLIN;
                  if (!i->second[1].empty())
                  {
                    fds[unIndex].events |= POLLOUT;
                  }
                  unIndex++;
                }
                if ((nReturn = poll(fds, unIndex, 250)) > 0)
                {
                  // {{{ accept
                  if (fds[0].revents & POLLIN)
                  {
                    int fdData;
                    sockaddr_un cli_addr;
                    socklen_t clilen = sizeof(cli_addr);
                    if ((fdData = accept(fdUnix, (struct sockaddr *)&cli_addr, &clilen)) >= 0)
                    {
                      vector<string> buffer = {"", ""};
                      clients[fdData] = buffer;
                      buffer.clear();
                    }
                    else
                    {
                      bExit = true;
                    }
                  }
                  // }}}
                  for (size_t i = 1; i < unIndex; i++)
                  {
                    // {{{ read
                    if (fds[i].revents & POLLIN)
                    {
                      if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                      {
                        clients[fds[i].fd][0].append(szBuffer, nReturn);
                        while ((unPosition = clients[fds[i].fd][0].find("\n")) != string::npos)
                        {
                          bool bProcessed = false;
                          Json *ptJson = new Json(clients[fds[i].fd][0].substr(0, unPosition));
                          clients[fds[i].fd][0].erase(0, (unPosition + 1));
                          strError.clear();
                          if (ptJson->m.find("Action") != ptJson->m.end() && !ptJson->m["Action"]->v.empty())
                          {
                            list<string> keys;
                            Json *ptData;
                            if (ptJson->m.find("Keys") != ptJson->m.end())
                            {
                              for (list<Json *>::iterator j = ptJson->m["Keys"]->l.begin(); j != ptJson->m["Keys"]->l.end(); j++)
                              {
                                keys.push_back((*j)->v);
                              }
                            }
                            if (ptJson->m.find("Data") != ptJson->m.end())
                            {
                              ptData = new Json(ptJson->m["Data"]);
                            }
                            else
                            {
                              ptData = new Json;
                            }
                            if (pStorage->request(ptJson->m["Action"]->v, keys, ptData, strError))
                            {
                              bProcessed = true;
                              ptJson->insert("Data", ptData);
                            }
                            keys.clear();
                            delete ptData;
                          }
                          else
                          {
                            strError = "Please provide the Action.";
                          }
                          ptJson->insert("Status", ((bProcessed)?"okay":"error"));
                          if (!strError.empty())
                          {
                            ptJson->insert("Error", strError);
                          }
                          clients[fds[i].fd][1].append(ptJson->json(strJson)+"\n");
                          delete ptJson;
                        }
                      }
                      else
                      {
                        removals.push_back(fds[i].fd);
                      }
                    }
                    // }}}
                    // {{{ write
                    if (fds[i].revents & POLLOUT)
                    {
                      if ((nReturn = write(fds[i].fd, clients[fds[i].fd][1].c_str(), clients[fds[i].fd][1].size())) > 0)
                      {
                        clients[fds[i].fd][1].erase(0, nReturn);
                      }
                      else
                      {
                        removals.push_back(fds[i].fd);
                      }
                    }
                    // }}}
                  }
                }
                else if (nReturn < 0)
                {
                  bExit = true;
                  ssMessage.str("");
                  ssMessage << strPrefix << "->poll(" << errno << ") error:  " << strerror(errno);
                  gpCentral->log(ssMessage.str());
                }
                removals.sort();
                removals.unique();
                while (!removals.empty())
                {
                  close(removals.front());
                  clients[removals.front()].clear();
                  clients.erase(removals.front());
                  removals.pop_front();
                }
                delete[] fds;
              }
              for (map<int, vector<string> >::iterator i = clients.begin(); i != clients.end(); i++)
              {
                i->second.clear();
                close(i->first);
              }
              clients.clear();
              if (!gbShutdown)
              {
                gbShutdown = true;
              }
              delete pStorage;
            }
            else
            {
              ssMessage.str("");
              ssMessage << strPrefix << "->listen(" << errno << ") error:  " << strerror(errno);
              gpCentral->alert(ssMessage.str());
            }
            close(fdUnix);
            ssMessage.str("");
            ssMessage << strPrefix << ":  Closed the socket.";
            gpCentral->log(ssMessage.str());
          }
          else
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->bind(" << errno << ") error:  " << strerror(errno);
            gpCentral->alert(ssMessage.str());
          }
        }
        else
        {
          ssMessage.str("");
          ssMessage << strPrefix << "->socket(" << errno << ") error:  " << strerror(errno);
          gpCentral->alert(ssMessage.str());
        }
        gpCentral->file()->remove((gstrData + STORAGE_SOCKET).c_str());
      }
      // }}}
      // {{{ error
      else
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->fork(" << errno << ") error:  " << strerror(errno);
        gpCentral->alert(ssMessage.str());
      }
      // }}}
      // {{{ check pid file
      if (gpCentral->file()->fileExist((gstrData + PID).c_str()))
      {
        gpCentral->file()->remove((gstrData + PID).c_str());
      }
      // }}}
    }
  }
  // }}}
  // {{{ usage statement
  else
  {
    mUSAGE(argv[0]);
  }
  // }}}
  delete gpCentral;

  return 0;
}
// }}}
// {{{ sighandle()
void sighandle(const int nSignal)
{
  string strError, strSignal;
  stringstream ssSignal;

  sethandles(sigdummy);
  gbShutdown = true;
  if (nSignal != SIGINT && nSignal != SIGTERM)
  {
    ssSignal << nSignal;
    gpCentral->alert((string)"The program's signal handling caught a " + (string)sigstring(strSignal, nSignal) + (string)"(" + ssSignal.str() + (string)")!  Exiting...", strError);
  }
  exit(1);
}
// }}}
// {{{ storage()
bool storage(const string strAction, list<string> keys, Json *ptData, string &strError)
{
  bool bResult = false;
  int fdUnix = -1, nReturn;

  if ((fdUnix = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0)
  {
    sockaddr_un addr;
    memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, (gstrData + STORAGE_SOCKET).c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fdUnix, (sockaddr *)&addr, sizeof(sockaddr_un)) == 0)
    {
      bool bExit = false;
      char szBuffer[65536];
      size_t unPosition;
      string strBuffer[2], strJson;
      Json *ptJson = new Json;
      ptJson->insert("Action", strAction);
      ptJson->insert("Keys", keys);
      if (ptData != NULL)
      {
        ptJson->insert("Data", ptData);
      }
      ptJson->json(strBuffer[1]);
      delete ptJson;
      strBuffer[1].append("\n");
      while (!bExit)
      {
        pollfd fds[1];
        fds[0].fd = fdUnix;
        fds[0].events = POLLIN;
        if (!strBuffer[1].empty())
        {
          fds[0].events |= POLLOUT;
        }
        if ((nReturn = poll(fds, 1, 250)) > 0)
        {
          // {{{ read
          if (fds[0].revents & POLLIN)
          {
            if ((nReturn = read(fdUnix, szBuffer, 65536)) > 0)
            {
              strBuffer[0].append(szBuffer, nReturn);
              if ((unPosition = strBuffer[0].find("\n")) != string::npos)
              {
                bExit = true;
                ptJson = new Json(strBuffer[0].substr(0, unPosition));
                strBuffer[0].erase(0, unPosition + 1);
                if (ptJson->m.find("Status") != ptJson->m.end() && ptJson->m["Status"]->v == "okay")
                {
                  bResult = true;
                  if (ptData != NULL && ptJson->m.find("Data") != ptJson->m.end())
                  {
                    ptData->parse(ptJson->m["Data"]->json(strJson));
                  }
                }
                else if (ptJson->m.find("Error") != ptJson->m.end() && !ptJson->m["Error"]->v.empty())
                {
                  strError = ptJson->m["Error"]->v;
                }
                else
                {
                  strError = "Encountered an unknown error.";
                }
                delete ptJson;
              }
            }
            else
            {
              bExit = true;
              if (nReturn < 0)
              {
                stringstream ssError;
                ssError << "read(" << errno << ") " << strerror(errno);
                strError = ssError.str();
              }
            }
          }
          // }}}
          // {{{ write
          if (fds[0].revents & POLLOUT)
          {
            if ((nReturn = write(fdUnix, strBuffer[1].c_str(), strBuffer[1].size())) > 0)
            {
              strBuffer[1].erase(0, nReturn);
            }
            else
            {
              bExit = true;
              if (nReturn < 0)
              {
                stringstream ssError;
                ssError << "write(" << errno << ") " << strerror(errno);
                strError = ssError.str();
              }
            }
          }
          // }}}
        }
        else if (nReturn < 0)
        {
          stringstream ssError;
          bExit = true;
          ssError << "poll(" << errno << ") " << strerror(errno);
          strError = ssError.str();
        }
      }
    }
    else
    {
      stringstream ssError;
      ssError << "connect(" << errno << ") " << strerror(errno);
      strError = ssError.str();
    }
    close(fdUnix);
  }
  else
  {
    stringstream ssError;
    ssError << "socket(" << errno << ") " << strerror(errno);
    strError = ssError.str();
  }

  return bResult;
}
// }}}
