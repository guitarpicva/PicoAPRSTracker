#include "uikissutils.h"
#include "ez_cpp_utils.h"

/**
     * Wrap a frame in KISS and return as std::string. Many thanks to John
     * Langner WB2OSZ since I based this on his C code from Direwolf v1.2.
     *
     * @param in - input array of bytes representing the AX.25 UI frame.
     * @return - std::string of KISS wrapped data.
     */
std::string UIKISSUtils::kissWrap(const std::string in)
{
    /**
     * The KISS "FEND" (frame end) character. One at each end of a KISS frame
     */
    const char FEND = 0xC0;
    /**
     * The KISS "FESC" (frame escape) character. Used to escape "FEND" inside a
     * KISS frame.
     */
    const char FESC = 0xDB;
    /**
     * The KISS "TFEND" character.
     */
    const char TFEND = 0xDC;
    /**
     * The KISS "TFESC" character.
     */
    const char TFESC = 0xDD;
    // John Langner's way from C in Direwolf 1.2
    // add a few indexes in case there needs to be any
    // escaped FENDs for FESCs
    // This is done the Qt way with std::std::stri for
    // inclusion in Qt projects.  Avoids array arithmetic
    //int[] cout = new int[in.length + 25];
    std::string out;

    out.append(1, FEND); // the opening FRAME END
     // the C way
    /* int olen = 0;
    cout[olen++] = FEND;
    for (int j = 0; j < in.length; j++) {
        if (in[j] == FEND) {
            cout[olen++] = FESC;
            cout[olen++] = TFEND;
        }
        else if (in[j] == FESC) {
            cout[olen++] = FESC;
            cout[olen++] = TFESC;
        }
        else {
            cout[olen++] = in[j];
        }
    */
   // the C++ way
    for (int i = 0; i < in.length(); i++) {
        //qDebug() << "Char:" << in.at(i);
        if (in.at(i) == FEND) {
            out.append(1, FESC).append(1, TFEND);
        }
        else if (in.at(i) == FESC) {
            out.append(1, FESC).append(1, TFESC);
        }
        else {
            out.append(1, in.at(i));
        }

    }
    //cout[olen++] = FEND; // close the output the C way
    out.append(1, FEND); // close the output the C++ way
    // control byte inserted late since it's a null
    out.insert(1, 1, 0x00);
    //return Arrays.copyOf(cout, olen); // the C way
    return out; // the C++ way
}

/**
     * Wrap a command in KISS and return as std::string.
     *
     * @param in - value to send with the command code.
     * @param cmdCode = the integer command code to send.
     * @return - std::string of KISS wrapped data.
     */
std::string UIKISSUtils::kissWrapCommand(const std::string val, const unsigned char cmdCode)
{
    
    // John Langner's way from C in Direwolf 1.2
    // add a few indexes in case there needs to be any
    // escaped FENDs for FESCs
    // This is done the Qt way with std::string for
    // inclusion in Qt projects.  Avoids array arithmetic
    //int[] cout = new int[in.length + 25];
    std::string out;

    out.append(1, FEND); // the opening FRAME END
    // int olen = 0;
    //cout[olen++] = FEND;
    //for (int j = 0; j < in.length; j++) {
    for (int i = 0; i < val.length(); i++) {
        //qDebug() << "Char:" << in.at(i);
        if (val.at(i) == FEND) {
            out.append(1, FESC).append(1, TFEND);
        }
        else if (val.at(i) == FESC) {
            out.append(1, FESC).append(1, TFESC);
        }
        else {
            out.append(1, val.at(i));
        }
    }
    out.append(1, FEND);
    // control byte inserted late since it's a null

    out.insert(1, 1, cmdCode); // caller is responsible for ensuring cmd code is valid.
    return out;
}

/**
     *
     * This method gets rid of the KISS frame ends and frame escapes
     * and returns the scrubbed array of characters for further processing. The
     * input is expected to contain an optional FEND, command type, and content
     * followed by a FEND. Once the KISS frame is unwrapped, it should contain
     * an AX.25 frame to be processed according to those rulesets.  Alternately
     * for KISS hosts that allow RAW text, the text will simply be ASCII chars.
     *
     * @param in - input array representation an AX.25 frame to be wrapped in KISS
     * @return - returns std::string of unwrapped AX.25 UI frame or RAW ASCII
     */
std::string UIKISSUtils::kissUnwrap(const std::string in)
{
    // John Langner's way fromn Direwolf 1.2
    std::string out;
    //int olen = 0;
    int ilen = in.length();
    printf("\nilen: %d", ilen);
    int j;
    bool escaped_mode = false;

    // Output array length will be less than input array length because we
    // are removing at least two FEND's
    //int[] out = new int[ilen];
    if (ilen < 2) {
        /* Need at least the "type indicator" char and FEND. */
        /* Probably more. */
        printf("\nEMPTY FRAME!");
        return out; // empty byte array indicates error
    }
    // If the last char is c0 (FEND), (which it properly should be) don't worry
    // about processing it later so deduct one from length of input array.
    if (in[ilen - 1] == FEND) {
        ilen--;
    }
    else {
        //qDebug() << "KISS frame should end with FEND.";
        printf("\nNO FEND on the END!");
        return out; // fail is returning empty std::string
    }
    //printf("\nilen adj:%d\n", ilen); // good
    // If the opening char is c0 (FEND) then we don't need to start
    // processing there either, so only deal with the actual AX.25 frame
    // contents.
    if (in[0] == FEND) {
        j = 1;
    }
    else {
        // otherwise, sometimes we don't get that opening FEND if frames are
        // glued together, so start at zero.
        j = 0;
    }
    //printf("j:%d\n", j);
    for (; j < ilen; j++) {
        //printf("%02x\n", in[j]);
        // According to the KISS spec, no un-escaped FEND's in the middle of
        // the AX.25 frame characters.
        if (in[j] == FEND) {
            //qDebug() << "KISS frame should not have FEND in the middle.";
            printf("\nUNESCAPED FEND!");
            out.clear();
            return out;
        }
        // Escaped mode means we found a FESC down below in the else if and
        // need to remove the following TFEND or TFESC character in the next
        // iteration of the loop above.
        if (escaped_mode) {
            if (in[j] == TFESC) {
                // Replace TFESC with FESC in output
                //out[olen++] = FESC;
                out.append(1, FESC);
            }
            else if (in[j] == TFEND) {
                // Replace TFEND with FEND in output
                //out[olen++] = FEND;
                out.append(1, FEND);
            }
            else {
                // If we had a FESC and it was not followed by a TFEND or a
                // TFESC, then that's an error.
                //qDebug() << "KISS protocol error.  Found ";
                //printf("%02x ", in[j]);
                //System.err.println("after " + FESC);
                printf("\nESCAPE with no TFEND/TFESC!");
                out.clear();
                return out;
            }
            escaped_mode = false;
        }
        else if (in[j] == FESC) {
            // If a FESC is found, skip it but set the escape mode flag so
            // we deal with the next char properly as above explains.
            escaped_mode = true;
        }
        else {
            // Otherwise, it's a normal character so write it to the output
            // array.
            //out[olen++] = in[j];
            out.append(1, in[j]);
            //printf("%s\n", out.c_str());            
        }
    }
    if (out[0] == '\0')
        out.erase(0, 1); // remove leading NULL
    printf("\nout:%s\n", out.c_str());
    return out;
}

std::string UIKISSUtils::buildUIFrame(std::string dest_call, std::string source_call, std::string digi1, std::string digi2, std::string text)
{
    /* ax.25 UI frame has 7 chars for dest, 7 chars for source, 7 chars for
    * digi, (7 chars for a second digi) one byte for frame type of UI (03), and one byte for PID (f0)
    */
    // BEFORE GOING ANY FURTHER, Check to see if we have a first digi
    bool hasNoDigi = digi1.empty(); // if digi1 is empty, digi2 is moot
    // GET THE SECOND DIGI EXISTENCE FIRST
    bool hasNoDigi2 = hasNoDigi;
    if (!hasNoDigi2) // if false, double-check the second one
        hasNoDigi2 = digi2.empty();

    std::string out; // output buffer
    // Destination call sign SSID evaluation
    int D_SSID = 0;
    // NEED TO USE OUR SPLIT LIBRARY HERE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    std::vector<std::string> parts;
    split(dest_call, parts, '-');
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (parts.size() > 1) // there is an SSID
    {
        D_SSID = atoi(parts.at(1).data());
    }
    // if no SSID, then the whole thing is call sign in parts[0]
    std::string msg = parts.at(0).substr(0, 6);
    // max 6 chars for Dest Call sign
    for (int i = 0; i < 6; i++) {
        if (i < dest_call.length()) {
            out.append(1, (unsigned char) msg[i] << 1);
        }
        else {
            out.append(1, (unsigned char) 0x40); // bit shifted space 0x20
        }
    }
    if (D_SSID == 0) {
        // Dest 0000 SSID char with Control bit set
        out.append(1, (unsigned char) 0xe0);
    }
    else {
        // build the SSID into the Dest Address field
        unsigned char val = (unsigned char) D_SSID;
        //qDebug() << "OTHER SSID:" << Qstd::stri::number(val, 2);
        val = val << 1; // move the SSID left one bit
        //qDebug() << "SHIFT SSID:" << Qstd::stri::number(val, 2);
        val = val | 0xe0; // bit mask "11100000", last bit unset since Dest address
        //qDebug() << "MASK SSID:" << Qstd::stri::number(val, 2);
        out.append(1, val);
    }

    // SOURCE Call sign processing
    D_SSID = 0; // start with SSID of 0

    // use an iterator to erase all parts (2)
    parts.erase(parts.begin(), parts.end()); // clear the vector, but probably not necesssary??????
    
    // re-using parts here...be careful of the results
    split(source_call, parts, '-');
    if (parts.size() > 1) // there is an SSID
    {
        D_SSID = atoi(parts.at(1).data());
    }
    // re-use msg for source and digi
    msg = source_call.substr(0, 6);
    for (int i = 0; i < 6; i++) {
        if (i < source_call.length()) {
            out.append(1, (unsigned char) msg[i] << 1);
        }
        else {
            out.append(1, (unsigned char) 0x40);
        }
    }

    if (D_SSID == 0) {
        if (hasNoDigi) { // this will be the final address, so set the bit
            out.append(1, (unsigned char) 0x61);
        }
        else {
            // Dest 0000 SSID char with Control bit set
            out.append(1, (unsigned char) 0x60);
        }
    }
    else {
        // build the SSID into the Dest Address field
        unsigned char val = (unsigned char) D_SSID;
        //qDebug() << "OTHER SSID:" << Qstd::stri::number(val, 2);
        val = val << 1; // move the SSID left one bit
        //qDebug() << "SHIFT SSID:" << Qstd::stri::number(val, 2);
        if (hasNoDigi) {
            val = val | 0x61; // bit mask "01100001" since no digi follows Source address
            // this will be the final address, so set the bit
        }
        else {
            val = val | 0x60; // bit mask "01100000", last bit unset since Source address
        }
        //qDebug() << "MASK SSID:" << Qstd::stri::number(val, 2);
        out.append(1, val);
    } // END Source Call Sign processing

    // If there is a first digi, add it and it's SSID now
    if (!hasNoDigi) {
        // now add digi address
        //qDebug() << "DIGI:" << digi;
        D_SSID = 0;
        parts.erase(parts.begin(), parts.end());
        split(digi1, parts, '-');
        if (parts.size() > 1) {
            D_SSID = atoi(parts.at(1).data());
        }
        // get the Digi call sign
        msg = parts.at(0);
        for (int i = 0; i < 6; i++) {
            if (i < msg.length()) {
                out.append(1, (unsigned char) msg[i] << 1);
            }
            else {
                out.append(1, (unsigned char) 0x40); // bit shifted space 0x20
            }
        }
        //qDebug() << "DIGI:" << msg << "D_SSID" << D_SSID;

        // no SSID in the address so encode 0x61
        if (D_SSID == 0) {
            if (hasNoDigi2)
                out.append(1, (unsigned char) 0x61);
            else
                out.append(1, (unsigned char) 0x60);
            //qDebug() << "ZERO SSID:" << Qstd::stri::number(0x61, 2);
        }
        else {
            // encode SSID in bits 3-6 and LSB = 1
            // "H" bit NOT set, so 3 MSB's are 011
            // 011 SSID
            // LSB is 1 to mark final digi in list
            //System.out.println("011" + std::stri.format("%4s", Integer.toBinarystd::stri(D_SSID)).replace(' ', '0') + "1");
            //out[21] = Integer.parseInt("011" + std::stri.format("%4s", Integer.toBinarystd::stri(D_SSID)).replace(' ', '0') + "1", 2);
            unsigned char val = (unsigned char) D_SSID;
            //qDebug() << "OTHER SSID:" << Qstd::stri::number(val, 2);
            val = val << 1; // move the SSID left one bit
            //qDebug() << "SHIFT SSID:" << Qstd::stri::number(val, 2);
            if (hasNoDigi2)       // this is the last address so set the bit
                val = val | 0x61; // bit mask "01100001"
            else
                val = val | 0x60; // bit mask "01100000"
            //qDebug() << "MASK SSID:" << Qstd::stri::number(val, 2);
            out.append(1, val);
            //qDebug() << "OTHER SSID:" << Qstd::stri::number(val, 2);
        }
        //qDebug() << "SSID:" << Qstd::stri::number(out[21]);
    } // END digi encoding
    // If there is a second digi, add it and it's SSID now
    if (!hasNoDigi2) {
        // now add digi address
        //qDebug() << "DIGI:" << digi;
        D_SSID = 0;
        parts.erase(parts.begin(), parts.end());
        split(digi2, parts, '-');
        if (parts.size() > 1) {
            D_SSID = atoi(parts.at(1).data());
        }
        // get the Digi call sign
        msg = parts.at(0);;
        for (int i = 0; i < 6; i++) {
            if (i < msg.length()) {
                out.append(1, (unsigned char) msg[i] << 1);
            }
            else {
                out.append(1, (unsigned char) 0x40); // bit shifted space 0x20
            }
        }
        //qDebug() << "DIGI:" << msg << "D_SSID" << D_SSID;
        // the last address field so LSB is a 1 (UIChat only allows one digi)
        // no SSID in the address so encode 0x61
        if (D_SSID == 0) {
            out.append(1, (unsigned char) 0x61);
            //qDebug() << "ZERO SSID:" << Qstd::stri::number(0x61, 2);
        }
        else {
            // encode SSID in bits 3-6 and LSB = 1
            // "H" bit NOT set, so 3 MSB's are 011
            // 011 SSID
            // LSB is 1 to mark final digi in list
            //System.out.println("011" + std::stri.format("%4s", Integer.toBinarystd::stri(D_SSID)).replace(' ', '0') + "1");
            //out[21] = Integer.parseInt("011" + std::stri.format("%4s", Integer.toBinarystd::stri(D_SSID)).replace(' ', '0') + "1", 2);
            unsigned char val = (unsigned char) D_SSID;
            //qDebug() << "OTHER SSID:" << Qstd::stri::number(val, 2);
            val = val << 1; // move the SSID left one bit
            //qDebug() << "SHIFT SSID:" << Qstd::stri::number(val, 2);
            val = val | 0x61; // bit mask "01100001"
            //qDebug() << "MASK SSID:" << Qstd::stri::number(val, 2);
            out.append(1, val);
            //qDebug() << "OTHER SSID:" << Qstd::stri::number(val, 2);
        }
        //qDebug() << "SSID:" << Qstd::stri::number(out[21]);
    } // END digi encoding
    // Now encode the payload text
    // Control field
    // PID = 0x03, no layer 3
    out.append(1, (unsigned char) 0x03);
    out.append(1, (unsigned char) 0xf0);
    msg = text.substr(0, 256); // limit to 256 bytes
    for (int i = 0; i < msg.length(); i++) {
        out.append(1, (unsigned char) msg[i]);
    }
    //qDebug() << "OUT:" << out;
    return out;
}
