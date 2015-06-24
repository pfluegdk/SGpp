/*
 * ConfigurationParser.hpp
 *
 *  Created on: Mar 25, 2015
 *      Author: pfandedd
 */

#pragma once

#include <vector>
#include <map>
#include <string>

#include <sgpp/globaldef.hpp>

namespace SGPP {
  namespace base {

    class ConfigurationParameters {
      protected:
        std::map<std::string, std::string> parameters;
      public:
        ConfigurationParameters();

        ConfigurationParameters(std::string fileName,
                                std::map<std::string, std::string> defaultParameters = std::map<std::string, std::string>());

        std::string operator[](std::string key);

        bool getAsBoolean(std::string key);
        uint64_t getAsUnsigned(std::string key);

      protected:
        void readFromMap(std::map<std::string, std::string> parametersMap);

        void readFromFile(std::string fileName);
      private:
        std::vector<std::string> split(const std::string& s, char delim);
    };

  }
}

