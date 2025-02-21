#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <sys/prctl.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

#include <CuLogger.h>
#include <CuStringMatcher.h>
#include <yaml-cpp/yaml.h>

using namespace std;
using namespace literals;
using namespace CU;
namespace fs = filesystem;

template <typename T>
bool eq_and_not_eq(T a, T b, T c)
{
  return (a == b) && (a != c);
}

template <typename T>
bool all_eq(T a, T b) { return (a == b); }

template <typename T, typename... Args>
bool all_eq(T a, T b, Args... args)
{
  return (a == b) && all_eq(a, args...);
}

template <typename T>
bool all_not_eq(T a, T b) { return (a != b); }

template <typename T, typename... Args>
bool all_not_eq(T a, T b, Args... args)
{
  return (a != b) && all_not_eq(a, args...);
}

template <typename T>
bool all_or_eq(T a, T b) { return (a == b); }

template <typename T, typename... Args>
bool all_or_eq(T a, T b, Args... args)
{
  return (a == b) || all_or_eq(a, args...);
}

template <typename T>
bool all_or_not_eq(T a, T b) { return (a != b); }

template <typename T, typename... Args>
bool all_or_not_eq(T a, T b, Args... args)
{
  return (a != b) || all_or_not_eq(a, args...);
}

#define CLOGE(...) Logger::Error(__VA_ARGS__)
#define CLOGW(...) Logger::Warn(__VA_ARGS__)
#define CLOGI(...) Logger::Info(__VA_ARGS__)
#define CLOGD(...) Logger::Debug(__VA_ARGS__)
#define CLOGV(...) Logger::Verbose(__VA_ARGS__)

string exec(const string_view &cmd)
{
  if (FILE *fp = popen(cmd.data(), "r"))
  {
    ostringstream stm;
    int c;
    while ((c = fgetc(fp)) != EOF)
    {
      stm.put(c);
    }
    pclose(fp);
    return stm.str();
  }
  return "";
}

void rmDir(const string_view &dir)
{
  try
  {
    if (fs::exists(dir) && fs::is_directory(dir))
    {
      for (const auto &path : fs::directory_iterator(dir))
      {
        fs::remove_all(path.path());
      }
    }
  }
  catch (const exception &e)
  {
    stringstream ss;
    ss << "Error while running: " << e.what();
    CLOGE(ss.str().c_str());
    Logger::Flush();
  }
}

void cleanApp(const string &app, bool multiUser, const string &userID = "0")
{
  if (multiUser)
  {
    rmDir("/storage/emulated/" + userID + "/Android/data/" + app + "/cache");
    rmDir("/data/user/" + userID + "/" + app + "/cache");
    rmDir("/data/user/" + userID + "/" + app + "/code_cache");
  }
  else
  {
    rmDir("/sdcard/Android/data/" + app + "/cache");
    rmDir("/data/data/" + app + "/cache");
    rmDir("/data/data/" + app + "/code_cache");
  }
  CLOGI(("Cleaned app cache: " + app).c_str());
}

void cleanDir(const string_view &dir, bool &cleanDotFile, vector<string> &fileWhitelist, vector<string> &filenameWhitelist, vector<string> &filenameBlacklist)
{
  this_thread::sleep_for(chrono::milliseconds(10));
  for (const auto &entry : fs::directory_iterator(dir))
  {
    bool skip = false;
    for (const string &whitelistFile : fileWhitelist)
    {
      fs::path parentPath = fs::canonical(fs::path(whitelistFile));
      fs::path filePath = fs::canonical(fs::path(dir));
      if (parentPath == filePath || mismatch(parentPath.begin(), parentPath.end(), filePath.begin()).first == parentPath.end())
      {
        CLOGI(("Skip clean file: " + entry.path().string()).c_str());
        skip = true;
      }
    }
    if (not skip)
    {
      for (const string &whitelistFilename : filenameWhitelist)
      {
        if (entry.path().filename().string().find(whitelistFilename) !=
            string::npos)
        {
          // CLOGI(("Skip clean file: " + entry.path().string()).c_str());
          skip = true;
        }
      }
      if (not skip)
      {
        if (cleanDotFile && fs::exists(entry.path().string()) &&
            string_view(entry.path().filename().string().data(), 1) == "." &&
            string_view(entry.path().filename().string()) != ".nomedia")
        {
          fs::remove_all(entry.path().string());
          CLOGI(("Cleaned file: " + entry.path().string()).c_str());
          continue;
        }
        for (const string &blacklistFilename : filenameBlacklist)
        {
          if (fs::exists(entry.path().string()) &&
              entry.path().filename().string().find(blacklistFilename) != string::npos)
          {
            fs::remove_all(entry.path().string());
            CLOGI(("Cleaned file: " + entry.path().string()).c_str());
            break;
          }
        }
        if (fs::exists(entry.path().string()) && fs::is_directory(entry))
        {
          cleanDir(entry.path().string(), cleanDotFile, fileWhitelist, filenameWhitelist, filenameBlacklist);
        }
      }
    }
  }
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    cout << "Invalid usage" << endl
         << endl
         << "Cache Cleaner(https://github.com/Cai-Ming-Yu/CMY-CacheCleaner)"
         << endl
         << endl
         << "Usage: " << argv[0] << " [yaml config file]" << endl;
    return -1;
  }

  prctl(PR_SET_NAME, "CacheCleaner", 0, 0, 0);
  strcpy(argv[0], "CacheCleaner");

  const string configFile = fs::canonical(fs::absolute(fs::path(argv[1]))).string();
  const string logPath = fs::canonical(fs::absolute(fs::path(argv[1])).parent_path()).string() + "/runtimeLog.txt";

  bool cleanAppCache, multiUser, cleanSdcard, cleanDotFile;
  string time, appMode, appWhitelist, appBlacklist;
  vector<string> searchExt, filenameWhitelist, filenameBlacklist, fileWhitelist, fileBlacklist;
  map<string, vector<string>> appFileBlacklist;

  map<char, int> timeUnitInSeconds = {{'s', 1}, {'m', 60}, {'h', 3600}, {'d', 86400}};

  unordered_map<string_view, int> appModes = {{"user", 1}, {"ystem", 2}, {"all", 3}};

  for (;;)
  {
    Logger::Create(Logger::LogLevel::INFO, logPath);
    CLOGI(("Created log file: " + logPath).c_str());
    Logger::Flush();

    cleanAppCache = multiUser = cleanSdcard = cleanDotFile = false;
    time = appMode = appWhitelist = appBlacklist = "";
    searchExt = filenameWhitelist = filenameBlacklist = fileWhitelist = fileBlacklist = {};
    appFileBlacklist = {};

    for (auto &str : {&time, &appMode, &appWhitelist, &appBlacklist})
    {
      (*str).clear();
      (*str).shrink_to_fit();
    }

    for (auto &str : {&searchExt, &filenameWhitelist, &filenameBlacklist, &fileWhitelist, &fileBlacklist})
      for (auto &s : *str)
      {
        s.clear();
        s.shrink_to_fit();
      }

    for (auto &pair : appFileBlacklist)
      for (auto &str : pair.second)
      {
        str.clear();
        str.shrink_to_fit();
      }

    try
    {
      YAML::Node config = YAML::LoadFile(configFile);
      CLOGI(("Read config file: " + configFile).c_str());

      time = config["time"].as<string>();
      CLOGI(("config time: " + time).c_str());

      cleanAppCache = config["cleanAppCache"].as<bool>();
      if (cleanAppCache)
      {
        CLOGI("config cleanAppCache: true");
      }
      else
      {
        CLOGI("config cleanAppCache: false");
      }

      appMode = config["appMode"].as<string>();
      CLOGI(("config appMode: " + appMode).c_str());

      cleanAppCache = config["multiUser"].as<bool>();
      if (cleanAppCache)
      {
        CLOGI("config multiUser: true");
      }
      else
      {
        CLOGI("config multiUser: false");
      }

      CLOGI("config appWhitelist:");
      for (const auto &app : config["appWhitelist"])
      {
        appWhitelist += "アプリ" + app.as<string>() + "アプリ";
        CLOGI(app.as<string>().c_str());
      }

      CLOGI("config appBlacklist:");
      for (const auto &app : config["appBlacklist"])
      {
        appBlacklist += "アプリ" + app.as<string>() + "アプリ";
        CLOGI(app.as<string>().c_str());
      }

      CLOGI("config appFileBlacklist:");
      const YAML::Node &appFileBlacklistNode = config["appFileBlacklist"];
      if (appFileBlacklistNode.IsDefined() && appFileBlacklistNode.IsMap())
      {
        for (const auto &app : appFileBlacklistNode)
        {
          string packageName = app.first.as<string>();
          CLOGI((packageName + ":").c_str());
          const YAML::Node &pathsNode = app.second;
          if (pathsNode.IsSequence())
          {
            for (const auto &path : pathsNode)
            {
              appFileBlacklist[packageName].push_back(path.as<string>());
              CLOGI(path.as<string>().c_str());
            }
          }
          else
          {
            appFileBlacklist[packageName].push_back(pathsNode.as<string>());
            CLOGI(pathsNode.as<string>().c_str());
          }
        }
      }

      cleanSdcard = config["cleanSdcard"].as<bool>();
      if (cleanSdcard)
      {
        if (multiUser)
        {
          for (const auto &entry : fs::directory_iterator("/storage/emulated"))
          {
            if (entry.is_directory())
            {
              searchExt.push_back("/storage/emulated/" + entry.path().filename().string());
            }
          }
        }
        else
        {
          searchExt.push_back("/sdcard");
        }
        CLOGI("config cleanSdcard: true");
      }
      else
      {
        CLOGI("config cleanSdcard: false");
      }

      CLOGI("config searchExt:");
      for (const auto &dir : config["earchExt"])
      {
        searchExt.push_back(dir.as<string>());
        CLOGI(dir.as<string>().c_str());
      }

      cleanDotFile = config["cleanDotFile"].as<bool>();
      if (cleanDotFile)
      {
        CLOGI("config cleanDotFile: true");
      }
      else
      {
        CLOGI("config cleanDotFile: false");
      }

      CLOGI("config filenameWhitelist:");
      for (const auto &filename : config["filenameWhitelist"])
      {
        filenameWhitelist.push_back(filename.as<string>());
        CLOGI(filename.as<string>().c_str());
      }

      CLOGI("config filenameBlacklist:");
      for (const auto &filename : config["filenameBlacklist"])
      {
        filenameBlacklist.push_back(filename.as<string>());
        CLOGI(filename.as<string>().c_str());
      }

      CLOGI("config fileWhitelist:");
      for (const auto &file : config["fileWhitelist"])
      {
        fileWhitelist.push_back(file.as<string>());
        CLOGI(file.as<string>().c_str());
      }

      CLOGI("config fileBlacklist:");
      for (const auto &file : config["fileBlacklist"])
      {
        fileBlacklist.push_back(file.as<string>());
        CLOGI(file.as<string>().c_str());
      }

      CLOGI("Read config finished");
      Logger::Flush();
    }
    catch (const YAML::Exception &e)
    {
      stringstream ss;
      ss << "Error while parsing YAML: " << e.what();
      CLOGE(ss.str().c_str());
      Logger::Flush();
      return -1;
    }

    try
    {
      if (cleanAppCache)
      {
        string apps;
        if (multiUser)
        {
          for (const auto &entry : fs::directory_iterator("/data/user"))
          {
            if (entry.is_directory())
            {
              string userID = entry.path().filename().string();
              switch (appModes[appMode])
              {
              case 1:
                CLOGI("Run cleanAppCache in user mode");
                apps = exec("pm list packages -3 --user " + userID + " | sed 's/package://'");
                break;
              case 2:
                CLOGI("Run cleanAppCache in system mode");
                apps = exec("pm list packages -s --user " + userID + " | sed 's/package://' | grep -v '^android$' | grep -v '^oplus$'");
                break;
              case 3:
                CLOGI("Run cleanAppCache in all mode");
                apps = exec("pm list packages --user " + userID + " | sed 's/package://' | grep -v '^android$' | grep -v '^oplus$'");
                break;
              default:
                CLOGW("Unknown appMode, cleanAppCache will not run");
                apps = "";
                break;
              }
              if (all_not_eq(apps, ""s, "\n"s))
              {
                istringstream iss(apps);
                string packageName;
                while (getline(iss, packageName, '\n'))
                {
                  StringMatcher matcher("*アプリ" + packageName + "アプリ*");
                  if (matcher.match(appWhitelist))
                  {
                    CLOGI(("Skip cleanup app: " + packageName).c_str());
                  }
                  else if (matcher.match(appBlacklist))
                  {
                    CLOGI(("Force cleanup app: " + packageName).c_str());
                    cleanApp(packageName, multiUser, userID);
                  }
                  else
                  {
                    if (all_or_eq(exec("pidof " + packageName + " 2>/dev/null"), ""s, "\n"s))
                    {
                      cleanApp(packageName, multiUser, userID);
                    }
                  }
                  this_thread::sleep_for(chrono::milliseconds(10));
                }
              }
            }
          }
        }
        else
        {
          switch (appModes[appMode])
          {
          case 1:
            CLOGI("Run cleanAppCache in user mode");
            apps = exec("pm list packages -3 | sed 's/package://'");
            break;
          case 2:
            CLOGI("Run cleanAppCache in system mode");
            apps = exec("pm list packages -s | sed 's/package://' | grep -v '^android$' | grep -v '^oplus$'");
            break;
          case 3:
            CLOGI("Run cleanAppCache in all mode");
            apps = exec("pm list packages | sed 's/package://' | grep -v '^android$' | grep -v '^oplus$'");
            break;
          default:
            CLOGW("Unknown appMode, cleanAppCache will not run");
            apps = "";
            break;
          }
          if (all_not_eq(apps, ""s, "\n"s))
          {
            istringstream iss(apps);
            string packageName;
            while (getline(iss, packageName, '\n'))
            {
              StringMatcher matcher("*アプリ" + packageName + "アプリ*");
              if (matcher.match(appWhitelist))
              {
                CLOGI(("Skip cleanup app: " + packageName).c_str());
              }
              else if (matcher.match(appBlacklist))
              {
                CLOGI(("Force cleanup app: " + packageName).c_str());
                cleanApp(packageName, multiUser);
              }
              else
              {
                if (all_or_eq(exec("pidof " + packageName + " 2>/dev/null"), ""s, "\n"s))
                {
                  cleanApp(packageName, multiUser);
                }
              }
              this_thread::sleep_for(chrono::milliseconds(10));
            }
          }
        }
      }
      Logger::Flush();

      for (auto &str : {&appMode, &appWhitelist, &appBlacklist})
      {
        (*str).clear();
        (*str).shrink_to_fit();
      }

      for (const auto &[app, file] : appFileBlacklist)
      {
        if (all_or_eq(exec("pidof " + app + " 2>/dev/null"), ""s, "\n"s))
        {
          for (const auto &path : file)
          {
            if (fs::exists(path))
            {
              fs::remove_all(path);
              CLOGI(("Cleaned file: " + path).c_str());
              this_thread::sleep_for(chrono::milliseconds(10));
            }
          }
        }
      }
      Logger::Flush();

      for (auto &pair : appFileBlacklist)
      {
        for (auto &str : pair.second)
        {
          str.clear();
          str.shrink_to_fit();
        }
      }

      for (const string &dir : searchExt)
      {
        bool skip = false;
        for (const string &whitelistFile : fileWhitelist)
        {
          fs::path parentPath = fs::canonical(fs::path(whitelistFile));
          fs::path filePath = fs::canonical(fs::path(dir));
          if (parentPath == filePath ||
              mismatch(parentPath.begin(), parentPath.end(), filePath.begin()).first == parentPath.end())
          {
            // CLOGI(("Skip clean file: " + dir).c_str());
            skip = true;
          }
        }
        if (not skip)
        {
          cleanDir(dir, cleanDotFile, fileWhitelist, filenameWhitelist, filenameBlacklist);
        }
        this_thread::sleep_for(chrono::milliseconds(10));
      }
      Logger::Flush();

      for (auto &str : {&searchExt, &filenameWhitelist, &filenameBlacklist, &fileWhitelist})
      {
        for (auto &s : *str)
        {
          s.clear();
          s.shrink_to_fit();
        }
      }

      for (const string &file : fileBlacklist)
      {
        if (fs::exists(file))
        {
          fs::remove_all(file);
          CLOGI(("Cleaned file: " + file).c_str());
          this_thread::sleep_for(chrono::milliseconds(10));
        }
      }

      for (auto &str : {&fileBlacklist})
      {
        for (auto &s : *str)
        {
          s.clear();
          s.shrink_to_fit();
        }
      }

      CLOGI(("Work finished, rest " + time).c_str());
      Logger::Flush();
      sleep(stoi(time.c_str()) * timeUnitInSeconds[time.back()]);
    }
    catch (const exception &e)
    {
      stringstream ss;
      ss << "Error while running: " << e.what();
      CLOGE(ss.str().c_str());
      Logger::Flush();
      return -1;
    }
  }

  return 0;
}