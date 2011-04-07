// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "CoreStringUtils.h"

std::wstring ToWString(const std::string &str)
{
    std::wstring w_str(str.length(), L' ');
    std::copy(str.begin(), str.end(), w_str.begin());
    return w_str;
}

std::string BufferToString(const std::vector<s8>& buffer)
{
    if (buffer.size())
        return std::string((const char*)&buffer[0], buffer.size());
    else
        return std::string();
}

std::vector<s8> StringToBuffer(const std::string& str)
{
    std::vector<s8> ret;
    ret.resize(str.size());
    if (str.size())
        memcpy(&ret[0], &str[0], str.size());
    return ret;
}

/// Get the current time as a string.
std::string GetLocalTimeString()
{
    return "";
/*
    Poco::LocalDateTime *time = new Poco::LocalDateTime();
    std::stringstream ss;
    
    ss << std::setw(2) << time->hour() << std::setfill('0') << ":" <<
        std::setw(2) << time->minute() << std::setfill('0') << ":" <<
        std::setw(2) << time->second() << std::setfill('0');
        
    SAFE_DELETE(time);
    
    return ss.str();
    */
}

/// Get the current date and time as a string.
std::string GetLocalDateTimeString()
{
    return "";
/*
    Poco::LocalDateTime *time = new Poco::LocalDateTime();
    std::stringstream ss;
    
    ss << std::setw(2) << time->day() << std::setfill('0') << "/" <<
        std::setw(2) << time->month() << std::setfill('0') << "/" <<
        std::setw(4) << time->year() << std::setfill('0') << " " <<
        std::setw(2) << time->hour() << std::setfill('0') << ":" <<
        std::setw(2) << time->minute() << std::setfill('0') << ":" <<
        std::setw(2) << time->second() << std::setfill('0');
    
    SAFE_DELETE(time);
    
    return ss.str();*/
}

StringVector SplitString(const std::string& str, char separator)
{
    std::vector<std::string> vec;
    unsigned pos = 0;

    while(pos < str.length())
    {
        unsigned start = pos;
        
        while(start < str.length())
        {
            if (str[start] == separator)
                break;
            
            start++;
        }

        if (start == str.length())
        {
            vec.push_back(str.substr(pos));
            break;
        }
        
        unsigned end = start;

        while(end < str.length())
        {
            if (str[end] != separator)
                break;

            end++;
        }

        vec.push_back(str.substr(pos, start - pos));
        pos = end;
    }

    return vec;
}

std::string ReplaceSubstring(const std::string &str, const std::string &replace_this, const std::string &replace_with)
{
    std::string ret = str;
    ReplaceSubstringInplace(ret, replace_this, replace_with);
    return ret;
}

std::string ReplaceChar(const std::string& str, char replace_this, char replace_with)
{
    std::string ret = str;
    ReplaceCharInplace(ret, replace_this, replace_with);
    return ret;
}      

void ReplaceSubstringInplace(std::string &str, const std::string &replace_this, const std::string &replace_with)
{
    std::size_t index = str.find(replace_this, 0);
    while(index != std::string::npos)
    {
        str.replace(index, replace_this.length(), replace_with);
        index = str.find(replace_this, 0);
    }
}


void ReplaceCharInplace(std::string& str, char replace_this, char replace_with)
{
    for(uint i = 0; i < str.length(); ++i)
        if (str[i] == replace_this) str[i] = replace_with;
}

uint GetHash(const std::string& str)
{
    uint ret = 0;
    
    if (!str.length())
        return ret;
    
    const char* cstr = str.c_str();
    
    while(*cstr)
    {
        // Note: calculate case-insensitive hash
        char c = *cstr;
        ret = tolower(c) + (ret << 6) + (ret << 16) - ret;
        ++cstr;
    }
    
    return ret;
}

uint GetHash(const QString& str)
{
    return GetHash(str.toStdString());
}

bool ParseBool(const std::string &value)
{
    std::string testedvalue = value;
    boost::algorithm::to_lower(testedvalue);
    return (boost::algorithm::starts_with(testedvalue,"true") || boost::algorithm::starts_with(testedvalue,"1")); 
}

bool ParseBool(const QString &value)
{
    return ParseBool(value.toStdString());
}

