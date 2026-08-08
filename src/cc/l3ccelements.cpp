#include "gsml3parser/cc/l3ccelements.h"
#include <sstream>
#include <iomanip>

namespace gsml3parser {

// ── L3BearerCapability ──────────────────────────────────────────────────

size_t L3BearerCapability::lengthV() const {
    return 1 + mOctet3a.size();
}

Expected<L3BearerCapability> L3BearerCapability::parse(BitReader& br) {
    L3BearerCapability result;

    auto r = br.readField(8); if (!r) return Expected<L3BearerCapability>::error(r.error());
    result.mOctet3 = static_cast<uint8_t>(r.value());

    if (result.mOctet3 & 0x10) {
        r = br.readField(8); if (!r) return Expected<L3BearerCapability>::error(r.error());
        result.mOctet3a.push_back(static_cast<uint8_t>(r.value()));
        r = br.readField(8); if (!r) return Expected<L3BearerCapability>::error(r.error());
        result.mOctet3a.push_back(static_cast<uint8_t>(r.value()));
    }

    return Expected<L3BearerCapability>::hold(std::move(result));
}

void L3BearerCapability::write(BitWriter& bw) const {
    bw.writeField(mOctet3, 8);
    for (const auto& b : mOctet3a) {
        bw.writeField(b, 8);
    }
}

void L3BearerCapability::text(std::ostream& os) const {
    os << "BearerCap[0x" << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(mOctet3);
    for (const auto& b : mOctet3a) {
        os << " 0x" << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    os << "]";
}

// ── L3SupportedCodecList ───────────────────────────────────────────────

size_t L3SupportedCodecList::lengthV() const {
    return (mGsmCodecs.empty() ? 0 : 2 + mGsmCodecs.size()) +
           (mUmtsCodecs.empty() ? 0 : 2 + mUmtsCodecs.size());
}

Expected<L3SupportedCodecList> L3SupportedCodecList::parse(BitReader& br, size_t lengthBytes) {
    L3SupportedCodecList result;
    size_t end = br.position() + lengthBytes * 8;

    while (br.position() + 16 <= end) {
        auto r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
        unsigned sysId = r.value();
        r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
        unsigned bitmapLen = r.value();

        size_t remainingBytes = (end - br.position()) / 8;
        unsigned fixedLen = bitmapLen;
        if (fixedLen > remainingBytes) fixedLen = static_cast<unsigned>(remainingBytes);

        if (sysId == 0) {
            for (unsigned i = 0; i < fixedLen; ++i) {
                r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
                result.mGsmCodecs.push_back(static_cast<uint8_t>(r.value()));
            }
        } else if (sysId == 4) {
            for (unsigned i = 0; i < fixedLen; ++i) {
                r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
                result.mUmtsCodecs.push_back(static_cast<uint8_t>(r.value()));
            }
        } else {
            for (unsigned i = 0; i < fixedLen; ++i) {
                r = br.readField(8); if (!r) return Expected<L3SupportedCodecList>::error(r.error());
            }
        }
    }

    return Expected<L3SupportedCodecList>::hold(std::move(result));
}

void L3SupportedCodecList::write(BitWriter& bw) const {
    if (!mGsmCodecs.empty()) {
        bw.writeField(0, 8);
        bw.writeField(static_cast<uint32_t>(mGsmCodecs.size()), 8);
        for (const auto& b : mGsmCodecs) {
            bw.writeField(b, 8);
        }
    }
    if (!mUmtsCodecs.empty()) {
        bw.writeField(4, 8);
        bw.writeField(static_cast<uint32_t>(mUmtsCodecs.size()), 8);
        for (const auto& b : mUmtsCodecs) {
            bw.writeField(b, 8);
        }
    }
}

void L3SupportedCodecList::text(std::ostream& os) const {
    os << "CodecList";
    if (!mGsmCodecs.empty()) {
        os << " GSM=";
        for (const auto& b : mGsmCodecs) {
            os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
    }
    if (!mUmtsCodecs.empty()) {
        os << " UMTS=";
        for (const auto& b : mUmtsCodecs) {
            os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
    }
}

// ── L3BCDDigits ─────────────────────────────────────────────────────────

static int bcdEncode(char c) {
    if (c == '*') return 10;
    if (c == '#') return 11;
    if (c >= '0' && c <= '9') return c - '0';
    return 0;
}

static char bcdDecode(int d) {
    if (d == 10) return '*';
    if (d == 11) return '#';
    if (d >= 0 && d <= 9) return static_cast<char>('0' + d);
    return '0';
}

L3BCDDigits::L3BCDDigits(const char* wDigits) {
    std::strncpy(mDigits, wDigits, maxDigits);
    mDigits[maxDigits] = '\0';
}

L3BCDDigits::L3BCDDigits(const L3BCDDigits& other) {
    std::memcpy(mDigits, other.mDigits, sizeof(mDigits));
}

Expected<void> L3BCDDigits::parse(BitReader& br, size_t numOctets, bool international) {
    mDigits[0] = '\0';
    size_t i = 0;
    if (international && i < maxDigits) mDigits[i++] = '+';

    for (size_t readOctets = 0; readOctets < numOctets && i < maxDigits; ++readOctets) {
        auto r = br.readField(4); if (!r) return Expected<void>::error(r.error()); unsigned d2 = r.value();
        r = br.readField(4); if (!r) return Expected<void>::error(r.error()); unsigned d1 = r.value();
        mDigits[i++] = bcdDecode(d1);
        if (d2 != 0x0f && i < maxDigits) mDigits[i++] = bcdDecode(d2);
    }
    mDigits[i] = '\0';

    return Expected<void>::hold();
}

void L3BCDDigits::write(BitWriter& bw) const {
    unsigned index = 0;
    unsigned numDigits = static_cast<unsigned>(std::strlen(mDigits));
    if (index < numDigits && mDigits[index] == '+') index++;
    while (index < numDigits) {
        if ((index + 1) < numDigits)
            bw.writeField(bcdEncode(mDigits[index + 1]), 4);
        else
            bw.writeField(0x0f, 4);
        bw.writeField(bcdEncode(mDigits[index]), 4);
        index += 2;
    }
}

size_t L3BCDDigits::lengthV() const {
    unsigned sz = static_cast<unsigned>(std::strlen(mDigits));
    if (sz > 0 && mDigits[0] == '+') sz--;
    return (sz / 2) + (sz % 2);
}

std::ostream& operator<<(std::ostream& os, const L3BCDDigits& digits) {
    os << digits.digits();
    return os;
}

// ── L3CalledPartyBCDNumber ─────────────────────────────────────────────

L3CalledPartyBCDNumber::L3CalledPartyBCDNumber(const char* wDigits)
    : mPlan(NumberingPlan::E164) {
    if (wDigits[0] == '+') {
        mType = TypeOfNumber::International;
        mDigits = L3BCDDigits(wDigits + 1);
    } else {
        mType = TypeOfNumber::Unknown;
        mPlan = NumberingPlan::Unknown;
        mDigits = L3BCDDigits(wDigits);
    }
}

size_t L3CalledPartyBCDNumber::lengthV() const {
    return 1 + mDigits.lengthV();
}

Expected<L3CalledPartyBCDNumber> L3CalledPartyBCDNumber::parse(BitReader& br, size_t lengthBytes) {
    L3CalledPartyBCDNumber result;

    auto r = br.readField(1); if (!r) return Expected<L3CalledPartyBCDNumber>::error(r.error()); // spare
    r = br.readField(3); if (!r) return Expected<L3CalledPartyBCDNumber>::error(r.error());
    result.mType = static_cast<TypeOfNumber>(r.value());
    r = br.readField(4); if (!r) return Expected<L3CalledPartyBCDNumber>::error(r.error());
    result.mPlan = static_cast<NumberingPlan>(r.value());

    size_t digitBytes = lengthBytes - 1;
    auto res = result.mDigits.parse(br, digitBytes, result.mType == TypeOfNumber::International);
    if (!res) return Expected<L3CalledPartyBCDNumber>::error(res.error());

    return Expected<L3CalledPartyBCDNumber>::hold(std::move(result));
}

void L3CalledPartyBCDNumber::write(BitWriter& bw) const {
    bw.writeField(1, 1);
    bw.writeField(static_cast<uint32_t>(mType), 3);
    bw.writeField(static_cast<uint32_t>(mPlan), 4);
    mDigits.write(bw);
}

void L3CalledPartyBCDNumber::text(std::ostream& os) const {
    os << "CalledParty[" << mDigits.digits() << "]";
}

// ── L3CallingPartyBCDNumber ─────────────────────────────────────────────

L3CallingPartyBCDNumber::L3CallingPartyBCDNumber(const char* wDigits)
    : mPlan(NumberingPlan::E164), mDigits(wDigits) {
    mType = (wDigits[0] == '+') ? TypeOfNumber::International : TypeOfNumber::National;
}

size_t L3CallingPartyBCDNumber::lengthV() const {
    return 1 + mDigits.lengthV() + (mHaveOctet3a ? 1 : 0);
}

Expected<L3CallingPartyBCDNumber> L3CallingPartyBCDNumber::parse(BitReader& br, size_t lengthBytes) {
    L3CallingPartyBCDNumber result;

    auto r = br.readField(1); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
    result.mHaveOctet3a = !r.value();
    r = br.readField(3); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
    result.mType = static_cast<TypeOfNumber>(r.value());
    r = br.readField(4); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
    result.mPlan = static_cast<NumberingPlan>(r.value());

    size_t remainingLength = lengthBytes - 1;
    if (result.mHaveOctet3a) {
        r = br.readField(1); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error()); // spare
        r = br.readField(2); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
        result.mPresentationIndicator = r.value();
        r = br.readField(3); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error()); // spare
        r = br.readField(2); if (!r) return Expected<L3CallingPartyBCDNumber>::error(r.error());
        result.mScreeningIndicator = r.value();
        remainingLength -= 1;
    }

    auto res = result.mDigits.parse(br, remainingLength, result.mType == TypeOfNumber::International);
    if (!res) return Expected<L3CallingPartyBCDNumber>::error(res.error());

    return Expected<L3CallingPartyBCDNumber>::hold(std::move(result));
}

void L3CallingPartyBCDNumber::write(BitWriter& bw) const {
    bw.writeField(mHaveOctet3a ? 0 : 1, 1);
    bw.writeField(static_cast<uint32_t>(mType), 3);
    bw.writeField(static_cast<uint32_t>(mPlan), 4);
    if (mHaveOctet3a) {
        bw.writeField(1, 1);
        bw.writeField(mPresentationIndicator, 2);
        bw.writeField(0, 3);
        bw.writeField(mScreeningIndicator, 2);
    }
    mDigits.write(bw);
}

void L3CallingPartyBCDNumber::text(std::ostream& os) const {
    os << "CallingParty[" << mDigits.digits() << "]";
}

// ── L3CauseElement ─────────────────────────────────────────────────────

L3CauseElement::L3CauseElement(Cause wCause, Location wLocation)
    : mLocation(wLocation), mCause(wCause) {}

Expected<L3CauseElement> L3CauseElement::parse(BitReader& br) {
    L3CauseElement result;

    auto r = br.readField(4); if (!r) return Expected<L3CauseElement>::error(r.error());
    result.mLocation = static_cast<Location>(r.value());
    r = br.readField(1); if (!r) return Expected<L3CauseElement>::error(r.error()); // spare
    r = br.readField(2); if (!r) return Expected<L3CauseElement>::error(r.error()); // coding standard
    r = br.readField(1); if (!r) return Expected<L3CauseElement>::error(r.error()); // ext
    r = br.readField(7); if (!r) return Expected<L3CauseElement>::error(r.error());
    result.mCause = static_cast<Cause>(r.value());
    r = br.readField(1); if (!r) return Expected<L3CauseElement>::error(r.error()); // ext

    return Expected<L3CauseElement>::hold(std::move(result));
}

void L3CauseElement::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mLocation), 4);
    bw.writeField(0, 1);
    bw.writeField(0x3, 2);
    bw.writeField(0, 1);
    bw.writeField(static_cast<uint32_t>(mCause), 7);
    bw.writeField(1, 1);
}

void L3CauseElement::text(std::ostream& os) const {
    os << "Cause[" << CCCause2Str(mCause) << "]";
}

// ── L3CallState ─────────────────────────────────────────────────────────

L3CallState::L3CallState(unsigned wCallState) : mCallState(wCallState) {}

Expected<L3CallState> L3CallState::parse(BitReader& br) {
    L3CallState result;

    auto r = br.readField(2); if (!r) return Expected<L3CallState>::error(r.error()); // spare
    r = br.readField(6); if (!r) return Expected<L3CallState>::error(r.error());
    result.mCallState = r.value();

    return Expected<L3CallState>::hold(std::move(result));
}

void L3CallState::write(BitWriter& bw) const {
    bw.writeField(3, 2);
    bw.writeField(mCallState, 6);
}

void L3CallState::text(std::ostream& os) const {
    os << "CallState[" << mCallState << "]";
}

// ── L3ProgressIndicator ────────────────────────────────────────────────

L3ProgressIndicator::L3ProgressIndicator(Progress wProgress, Location wLocation)
    : mLocation(wLocation), mProgress(wProgress) {}

Expected<L3ProgressIndicator> L3ProgressIndicator::parse(BitReader& br) {
    L3ProgressIndicator result;

    auto r = br.readField(4); if (!r) return Expected<L3ProgressIndicator>::error(r.error()); // IEI
    r = br.readField(4); if (!r) return Expected<L3ProgressIndicator>::error(r.error());
    result.mLocation = static_cast<Location>(r.value());
    r = br.readField(1); if (!r) return Expected<L3ProgressIndicator>::error(r.error()); // spare
    r = br.readField(7); if (!r) return Expected<L3ProgressIndicator>::error(r.error());
    result.mProgress = static_cast<Progress>(r.value());

    return Expected<L3ProgressIndicator>::hold(std::move(result));
}

void L3ProgressIndicator::write(BitWriter& bw) const {
    bw.writeField(0x0e, 4);
    bw.writeField(static_cast<uint32_t>(mLocation), 4);
    bw.writeField(1, 1);
    bw.writeField(static_cast<uint32_t>(mProgress), 7);
}

void L3ProgressIndicator::text(std::ostream& os) const {
    os << "Progress[location=" << static_cast<int>(mLocation)
       << " progress=" << static_cast<int>(mProgress) << "]";
}

// ── L3KeypadFacility ───────────────────────────────────────────────────

L3KeypadFacility::L3KeypadFacility(char wIA5) : mIA5(wIA5) {}

Expected<L3KeypadFacility> L3KeypadFacility::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3KeypadFacility>::error(r.error());
    return Expected<L3KeypadFacility>::hold(L3KeypadFacility(static_cast<char>(r.value())));
}

void L3KeypadFacility::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mIA5), 8);
}

void L3KeypadFacility::text(std::ostream& os) const {
    os << "Keypad[" << mIA5 << "]";
}

// ── L3Signal ────────────────────────────────────────────────────────────

L3Signal::L3Signal(SignalValues tone) : mSignalValue(tone) {}

Expected<L3Signal> L3Signal::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3Signal>::error(r.error());
    return Expected<L3Signal>::hold(L3Signal(static_cast<SignalValues>(r.value())));
}

void L3Signal::write(BitWriter& bw) const {
    bw.writeField(static_cast<uint32_t>(mSignalValue), 8);
}

void L3Signal::text(std::ostream& os) const {
    os << "Signal[0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(mSignalValue) << "]";
}

// ── L3RepeatIndicator ─────────────────────────────────────────────────

L3RepeatIndicator::L3RepeatIndicator(unsigned wValue) : mValue(wValue) {}

Expected<L3RepeatIndicator> L3RepeatIndicator::parse(BitReader& br) {
    auto r = br.readField(4); if (!r) return Expected<L3RepeatIndicator>::error(r.error());
    return Expected<L3RepeatIndicator>::hold(L3RepeatIndicator(r.value()));
}

void L3RepeatIndicator::write(BitWriter& bw) const {
    bw.writeField(mValue, 4);
}

void L3RepeatIndicator::text(std::ostream& os) const {
    os << "RepeatIndicator: " << mValue;
}

// ── L3SupServFacilityIE ─────────────────────────────────────────────────

L3SupServFacilityIE::L3SupServFacilityIE(const std::string& wData)
    : mData(wData) {}

size_t L3SupServFacilityIE::lengthV() const {
    return mData.size();
}

Expected<L3SupServFacilityIE> L3SupServFacilityIE::parse(BitReader& br, size_t lengthBytes) {
    std::string data;
    data.reserve(lengthBytes);
    for (size_t i = 0; i < lengthBytes; ++i) {
        auto r = br.readField(8); if (!r) return Expected<L3SupServFacilityIE>::error(r.error());
        data.push_back(static_cast<char>(r.value()));
    }
    return Expected<L3SupServFacilityIE>::hold(L3SupServFacilityIE(data));
}

Expected<L3SupServFacilityIE> L3SupServFacilityIE::parse(BitReader& br) {
    size_t remainingBits = br.remainingBits();
    size_t lengthBytes = remainingBits / 8;
    return parse(br, lengthBytes);
}

void L3SupServFacilityIE::write(BitWriter& bw) const {
    for (size_t i = 0; i < mData.size(); ++i) {
        bw.writeField(static_cast<uint32_t>(static_cast<unsigned char>(mData[i])), 8);
    }
}

void L3SupServFacilityIE::text(std::ostream& os) const {
    os << "Facility[";
    for (size_t i = 0; i < mData.size(); ++i) {
        os << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(static_cast<unsigned char>(mData[i]));
    }
    os << "]";
}

// ── L3SupServVersionIndicator ───────────────────────────────────────────

Expected<L3SupServVersionIndicator> L3SupServVersionIndicator::parse(BitReader& br) {
    auto r = br.readField(8); if (!r) return Expected<L3SupServVersionIndicator>::error(r.error());
    L3SupServVersionIndicator result;
    result.mVersion = r.value();
    return Expected<L3SupServVersionIndicator>::hold(std::move(result));
}

void L3SupServVersionIndicator::write(BitWriter& bw) const {
    bw.writeField(mVersion, 8);
}

void L3SupServVersionIndicator::text(std::ostream& os) const {
    os << "SSVersion[" << mVersion << "]";
}

} // namespace gsml3parser
