/**
 * BU86X.cpp — see BU86X.h for documentation
 *
 * PAYLOAD MAP (0-indexed, 24 bytes total)
 * ----------------------------------------
 * [0]     unused / report ID echo
 * [1]     mode flags: bit7=AVG bit6=MIN bit5=MAX bit4=R bit3=H bit0=AUTO
 * [2]     main AC/DC: bit2=~(AC) bit1=-(DC)
 * [3-8]   main display digit segments (6 digits)
 *           bit0 of each byte = decimal point AFTER that digit
 *           bit0 of byte[8] = V (Volt annunciator, main)
 * [9]     secondary flags: bit5=~(AC) bit4=-(DC) bit2=A bit1=m bit0=u
 * [10-13] secondary display digit segments (4 digits)
 * [14]    units: bit7=A bit6=n bit5=F bit4=S bit3=V② bit2=Hz② bit1=k② bit0=M②
 * [15]    units: bit6=k① bit5=M① bit4=Ohm bit3=u① bit2=m① bit1=dB bit0=Hz①
 * [16]    Model ID = 0x86  ← verify this
 * [17-23] don't care
 */

#include "BU86X.h"

static const uint8_t RTD[3] = { 0x00, 0x86, 0x66 };

BU86X* _bu86x_instance = nullptr;
static EspUsbHost _usb;

// ── begin ─────────────────────────────────────────────────────────
void BU86X::begin() {
    _bu86x_instance = this;
    memset(&_latest, 0, sizeof(_latest));
    _latest.mainValue = NAN;
    _latest.subValue  = NAN;

    _usb.onDeviceConnected([](const EspUsbHostDeviceInfo &d) {
        if (_bu86x_instance) _bu86x_instance->_onConnect(d.vid, d.pid, d.address);
    });
    _usb.onDeviceDisconnected([](const EspUsbHostDeviceInfo &d) {
        if (_bu86x_instance) _bu86x_instance->_onDisconnect(d.vid, d.pid);
    });
    _usb.onHIDInput([](const EspUsbHostHIDInput &i) {
        if (_bu86x_instance && i.vid == BU86X_VID && i.pid == BU86X_PID)
            _bu86x_instance->_onChunk(i.data, i.length);
    });
    _usb.onVendorInput([](const EspUsbHostVendorInput &i) {
        if (_bu86x_instance && i.vid == BU86X_VID && i.pid == BU86X_PID)
            _bu86x_instance->_onChunk(i.rawData, i.rawLength);
    });
    _usb.begin();
}

void BU86X::loop() {
    if (!_connected || _pollMs == 0) return;
    if (millis() - _lastTx >= _pollMs) { _lastTx = millis(); trigger(); }
}

bool BU86X::trigger() {
    if (!_connected) return false;
    return _usb.sendVendorOutput(RTD, sizeof(RTD), _devAddr);
}

bool BU86X::read(BU86X_Reading &out) {
    if (!_dataReady) return false;
    _dataReady = false;
    memcpy(&out, &_latest, sizeof(BU86X_Reading));
    return out.valid;
}

void BU86X::_onConnect(uint16_t vid, uint16_t pid, uint8_t addr) {
    if (vid != BU86X_VID || pid != BU86X_PID) return;
    _connected = true; _devAddr = addr; _rxLen = 0; _dataReady = false;
}
void BU86X::_onDisconnect(uint16_t vid, uint16_t pid) {
    if (vid != BU86X_VID || pid != BU86X_PID) return;
    _connected = false; _devAddr = 0;
}

void BU86X::_onChunk(const uint8_t *data, size_t len) {
    if (_debug) {
        Serial.printf("[BU86X] chunk %u: ", len);
        for (size_t i = 0; i < len; i++) { if (data[i]<16) Serial.print('0'); Serial.print(data[i],HEX); Serial.print(' '); }
        Serial.println();
    }
    uint8_t n = min((int)(BU86X_PAYLOAD - _rxLen), (int)len);
    memcpy(_rxBuf + _rxLen, data, n);
    _rxLen += n;
    if (_rxLen >= BU86X_PAYLOAD) { _decode(_latest); _dataReady = true; _rxLen = 0; }
}

// ── 7-segment decoder (ported from Python DM869sProcess.decodeDigit) ──
char BU86X::_seg7(uint8_t b) {
    switch (b >> 1) {
        case 0b1011111: return '0'; case 0b1010000: return '1';
        case 0b1101101: return '2'; case 0b1111100: return '3';
        case 0b1110010: return '4'; case 0b0111110: return '5';
        case 0b0111111: return '6'; case 0b1010100: return '7';
        case 0b1111111: return '8'; case 0b1111110: return '9';
        case 0b0001011: return 'L'; case 0b0000001: return '-';
        case 0b0000000: return ' '; default:         return '?';
    }
}

float BU86X::_toFloat(const char *d, int dp, bool neg) {
    char buf[20]={0}; int n=0, len=strlen(d);
    if (neg) buf[n++]='-';
    for (int i=0;i<len&&n<18;i++) { buf[n++]=d[i]; if(dp>0&&(i+1)==dp) buf[n++]='.'; }
    return atof(buf);
}

// ── Full decoder ──────────────────────────────────────────────────
void BU86X::_decode(BU86X_Reading &r) {
    const uint8_t *p = _rxBuf;
    memcpy(r.raw, p, BU86X_PAYLOAD);
    r.valid = (p[16] == BU86X_MODEL_ID);

    // Mode flags
    r.flag_avg  = (p[1]>>7)&1; r.flag_min  = (p[1]>>6)&1;
    r.flag_max  = (p[1]>>5)&1; r.flag_rel  = (p[1]>>4)&1;
    r.flag_hold = (p[1]>>3)&1; r.flag_auto = (p[1]>>0)&1;

    // Main AC/DC
    r.mainAC = (p[2]>>2)&1;
    r.mainDC = !r.mainAC && ((p[2]>>1)&1);

    // Main digits [3-8]
    char md[8]={0}; int mdp=0; bool mneg=false;
    for (int i=0;i<6;i++) {
        md[i] = _seg7(p[3+i]);
        if (!mdp && i<5 && (p[3+i+1]&0x01)) mdp=i+1;
    }
    r.mainOL=false; for(int i=0;i<6;i++) if(md[i]=='L'){r.mainOL=true;break;}

    char mc[8]={0}; int ci=0; bool lead=true;
    for(int i=0;i<6;i++){
        char c=md[i];
        if(lead&&c=='-'){mneg=true;continue;}
        if(lead&&c==' ')continue;
        lead=false;
        if(c>='0'&&c<='9') mc[ci++]=c;
    }
    r.mainNeg=mneg;

    // Main units
    bool uV=(p[8]>>0)&1, uA=(p[14]>>7)&1, uN=(p[14]>>6)&1;
    bool uF=(p[14]>>5)&1, uS=(p[14]>>4)&1;
    bool uK=(p[15]>>6)&1, uM=(p[15]>>5)&1, uMil=(p[15]>>2)&1;
    bool uU=(p[15]>>3)&1, uOhm=(p[15]>>4)&1, uHz=(p[15]>>0)&1, uDb=(p[15]>>1)&1;

    char pre[4]={0}, base[6]={0};
    if(uM) strcpy(pre,"M"); else if(uK) strcpy(pre,"k");
    else if(uMil) strcpy(pre,"m"); else if(uU) strcpy(pre,"u");
    else if(uN) strcpy(pre,"n");
    if(uV) strcpy(base,"V"); else if(uA) strcpy(base,"A");
    else if(uOhm) strcpy(base,"Ohm"); else if(uHz) strcpy(base,"Hz");
    else if(uF) strcpy(base,"F"); else if(uS) strcpy(base,"S");
    else if(uDb) strcpy(base,"dB");

    snprintf(r.mainUnit,sizeof(r.mainUnit),"%s%s%s",pre,base,
             r.mainAC?" AC":r.mainDC?" DC":"");

    if(r.mainOL){ r.mainValue=NAN; snprintf(r.mainDisplay,sizeof(r.mainDisplay),"OL %s",r.mainUnit); }
    else if(!ci){ r.mainValue=NAN; snprintf(r.mainDisplay,sizeof(r.mainDisplay),"--- %s",r.mainUnit); }
    else {
        r.mainValue=_toFloat(mc,mdp,mneg);
        char ns[16]={0}; int ni=0;
        if(mneg) ns[ni++]='-';
        int len=strlen(mc);
        for(int i=0;i<len&&ni<14;i++){ns[ni++]=mc[i];if(mdp>0&&(i+1)==mdp)ns[ni++]='.';}
        snprintf(r.mainDisplay,sizeof(r.mainDisplay),"%s %s",ns,r.mainUnit);
    }

    // Secondary AC/DC
    r.subAC=(p[9]>>5)&1; r.subDC=!r.subAC&&((p[9]>>4)&1);

    // Sub digits [10-13]
    char sd[6]={0}; int sdp=0; bool sneg=false;
    for(int i=0;i<4;i++){
        sd[i]=_seg7(p[10+i]);
        if(!sdp&&i<3&&(p[10+i+1]&0x01)) sdp=i+1;
    }
    r.subOL=false; for(int i=0;i<4;i++) if(sd[i]=='L'){r.subOL=true;break;}

    char sc[6]={0}; ci=0; lead=true;
    for(int i=0;i<4;i++){
        char c=sd[i];
        if(lead&&c=='-'){sneg=true;continue;}
        if(lead&&c==' ')continue;
        lead=false;
        if(c>='0'&&c<='9') sc[ci++]=c;
    }

    // Sub units
    bool sV=(p[14]>>3)&1, sHz=(p[14]>>2)&1, sK=(p[14]>>1)&1, sM2=(p[14]>>0)&1;
    bool sA=(p[9]>>2)&1, sMil=(p[9]>>1)&1, sU=(p[9]>>0)&1;

    char spre[4]={0}, sbase[6]={0};
    if(sM2) strcpy(spre,"M"); else if(sK) strcpy(spre,"k");
    else if(sMil) strcpy(spre,"m"); else if(sU) strcpy(spre,"u");
    if(sV) strcpy(sbase,"V"); else if(sA) strcpy(sbase,"A");
    else if(sHz) strcpy(sbase,"Hz");

    snprintf(r.subUnit,sizeof(r.subUnit),"%s%s%s",spre,sbase,
             r.subAC?" AC":r.subDC?" DC":"");

    if(r.subOL){ r.subValue=NAN; snprintf(r.subDisplay,sizeof(r.subDisplay),"OL %s",r.subUnit); }
    else if(!ci){ r.subValue=NAN; snprintf(r.subDisplay,sizeof(r.subDisplay),"--- %s",r.subUnit); }
    else {
        r.subValue=_toFloat(sc,sdp,sneg);
        char ns[12]={0}; int ni=0;
        if(sneg) ns[ni++]='-';
        int len=strlen(sc);
        for(int i=0;i<len&&ni<10;i++){ns[ni++]=sc[i];if(sdp>0&&(i+1)==sdp)ns[ni++]='.';}
        snprintf(r.subDisplay,sizeof(r.subDisplay),"%s %s",ns,r.subUnit);
    }
}

void BU86X::printReading(const BU86X_Reading &r) {
    Serial.printf("Main: %s  |  Sub: %s  |  Valid:%s\n",
                  r.mainDisplay, r.subDisplay, r.valid?"Y":"N");
}
