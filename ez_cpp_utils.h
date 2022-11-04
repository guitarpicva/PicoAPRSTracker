#include <string>
#include <vector>

// We all need to split a string on a delimiter!

// This version splits on a single char and keeps all empty tokens, so:
// split on "a,b,c,,,f,g,h" with ',' as the delimiter returns a vector 
// with size of 8 with two empty strings in indexes 3 and 4.
static void split_keep(std::string str, std::vector<std::string> &token_v, const char delim){
    size_t start = str.find_first_not_of(delim), end=start;
    while (start != std::string::npos){
        // Find next occurence of delimiter
        end = str.find(delim, start);
        // Push back the token found into vector
        // back to back tokens leave an empty string in the vector
        token_v.push_back(str.substr(start, end-start));
        // slide up one to get to the next starting point
        start = end + 1;
    }
}

// This version splits on a single char and discards all empty tokens, so:
// split on "a,b,c,,,f,g,h" with ',' as the delimiter returns a vector 
// with size of 6 containing <a b c f g h>
static void split_discard(std::string str, std::vector<std::string> &token_v, const char delim){
    size_t start = str.find_first_not_of(delim), end=start;
    while (start != std::string::npos && end != std::string::npos){
        // Find next occurence of delimiter
        end = str.find(delim, start);
        // Push back the token found into vector
        // back to back tokens leave an empty string in the vector
        token_v.push_back(str.substr(start, end-start));
        // slide up to the next non-delimiter char (skips empty fields)
        start = str.find_first_not_of(delim, end);
    }
}

// This version splits on a single char and keeps/discards all empty tokens
// based on bool passed. true = keep, false = discard
// with size of 6 containing <a b c f g h>
// Obviously, this could replace the two functions above if only dealing 
// by adding the bool parameter: default = true (keep empty tokens)
static void split(std::string str, std::vector<std::string> &token_v, const char delim, bool keep_empty_tokens = true){
    size_t start = str.find_first_not_of(delim), end=start;
    while (start != std::string::npos && end != std::string::npos){
        // Find next occurence of delimiter
        end = str.find(delim, start);
        // Push back the token found into vector
        // back to back tokens leave an empty string in the vector
        token_v.push_back(str.substr(start, end-start));
        // choose to keep or discard
        if(keep_empty_tokens) {
            start = end + 1;
        }
        else {
            // slide up to the next non-delimiter char (skips empty fields)
           start = str.find_first_not_of(delim, end);
        }
    }
}

// This version splits on a string token (finds a matching sub-string) 
// and keeps/discards all empty tokens based on bool passed. true = keep,
// false = discard
static void split(std::string str, std::vector<std::string> &token_v, const std::string delim, bool keep_empty_tokens = true){
    size_t start = str.find_first_not_of(delim), end=start;
    while (start != std::string::npos && end != std::string::npos){
        // Find next occurence of delimiter
        end = str.find(delim, start);
        // Push back the token found into vector
        // back to back tokens leave an empty string in the vector
        token_v.push_back(str.substr(start, end-start));
        // choose to keep or discard
        if(keep_empty_tokens) {
            start = end + 1;
        }
        else {
            // slide up to the next non-delimiter char (skips empty fields)
           start = str.find_first_not_of(delim, end);
        }
    }
}
/*
// Find the day of the year based on the system clock items year, month and day.
// Also accounts for leap years.
static uint16_t get_doy(const uint16_t year, const uint8_t mon, const uint8_t dom)
{
    uint8_t days_in_feb = 28, doy;    // day of year

    doy = dom; // start with the day of the current month

    // check for leap year
    if( (year % 4 == 0 && year % 100 != 0 ) || (year % 400 == 0) )
    {
        days_in_feb = 29;
    }

    switch(mon)
    { 
        // we don't have to check Jan since it will naturally be 1-31 as the DOY
        case 2:
            doy += 31;
            break;
        case 3:
            doy += 31+days_in_feb;
            break;
        case 4:
            doy += 31+days_in_feb+31;
            break;
        case 5:
            doy += 31+days_in_feb+31+30;
            break;
        case 6:
            doy += 31+days_in_feb+31+30+31;
            break;
        case 7:
            doy += 31+days_in_feb+31+30+31+30;
            break;            
        case 8:
            doy += 31+days_in_feb+31+30+31+30+31;
            break;
        case 9:
            doy += 31+days_in_feb+31+30+31+30+31+31;
            break;
        case 10:
            doy += 31+days_in_feb+31+30+31+30+31+31+30;            
            break;            
        case 11:
            doy += 31+days_in_feb+31+30+31+30+31+31+30+31;            
            break;                        
        case 12:
            doy += 31+days_in_feb+31+30+31+30+31+31+30+31+30;            
            break;                                    
    }

    return doy; // return 0 to operating system
}
*/