#ifndef UIKISSUTILS_H
#define UIKISSUTILS_H

#include <vector>
#include <string>

class UIKISSUtils
{
public:
    // explicit UIKISSUtils();
    /**
     * Convenience variables holding the value "UICHAT" and the chosen SSID
     * character, not currently used! Could be used later for a "chatroom" application
     * to have multiple activities at the same time in the UI space.
     */

    // const std::string UICHAT0 = std::string("\xaa\x92\x86\x90\x82\xa8\xe0");
    // const std::string UICHAT1 = std::string("\xaa\x92\x86\x90\x82\xa8\xe2");
    // const std::string UICHAT2 = std::string("\xaa\x92\x86\x90\x82\xa8\xe4");
    // const std::string UICHAT3 = std::string("\xaa\x92\x86\x90\x82\xa8\xe6");
    // const std::string UICHAT4 = std::string("\xaa\x92\x86\x90\x82\xa8\xe8");
    // const std::string UICHAT5 = std::string("\xaa\x92\x86\x90\x82\xa8\xea");
    // const std::string UICHAT6 = std::string("\xaa\x92\x86\x90\x82\xa8\xec");
    // const std::string UICHAT7 = std::string("\xaa\x92\x86\x90\x82\xa8\xee");
    // const std::string UICHAT8 = std::string("\xaa\x92\x86\x90\x82\xa8\xf0");
    // const std::string UICHAT9 = std::string("\xaa\x92\x86\x90\x82\xa8\xf2");
    // const std::string UICHAT10 = std::string("\xaa\x92\x86\x90\x82\xa8\xf4");
    // const std::string UICHAT11 = std::string("\xaa\x92\x86\x90\x82\xa8\xf6");
    // const std::string UICHAT12 = std::string("\xaa\x92\x86\x90\x82\xa8\xf8");
    // const std::string UICHAT13 = std::string("\xaa\x92\x86\x90\x82\xa8\xfa");
    // const std::string UICHAT14 = std::string("\xaa\x92\x86\x90\x82\xa8\xfc");
    // const std::string UICHAT15 = std::string("\xaa\x92\x86\x90\x82\xa8\xfe");

    /**
     * The KISS "FEND" (frame end) character. One at each end of a KISS frame
     */
    static const char FEND = 0xC0;
    /**
     * The KISS "FESC" (frame escape) character. Used to escape "FEND" inside a
     * KISS frame.
     */
    static const char FESC = 0xDB;
    /**
     * The KISS "TFEND" character.
     */
    static const char TFEND = 0xDC;
    /**
     * The KISS "TFESC" character.
     */
    static const char TFESC = 0xDD;

    static std::string kissWrap(const std::string in);
    static std::string kissWrapCommand(const std::string val, const unsigned char cmdCode);
    static std::string kissUnwrap(std::string in);
    static std::string buildUIFrame(
        std::string dest_call, std::string source_call, std::string digi1 = std::string(), std::string digi2 = std::string(), std::string text = std::string());
    static std::vector<std::string> unwrapUIFrame(std::string in);
};

#endif // UIKISSUTILS_H
