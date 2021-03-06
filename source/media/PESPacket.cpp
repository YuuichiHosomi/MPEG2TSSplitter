#include "media/PESPacket.hpp"

#include <iomanip>
#include "tools/SerializationImpl.hpp"
#include "tools/Console.hpp"

using namespace std;
using namespace Media;

constexpr uint32_t StartCodeValue = 0x000001;
constexpr uint8_t AudioStreamMask = 0xE0;
constexpr uint8_t AudioStreamID = 0xC0;
constexpr uint8_t VideoStreamMask = 0xF0;
constexpr uint8_t VideoStreamID = 0xE0;

PESPacket::PESPacket(IDecoder::Ptr decoder)
    : _startCode()
    , _streamID()
    , _packetLength()
    , _PESScramblingControl()
    , _PESPriority()
    , _dataAlignmentIndicator()
    , _copyright()
    , _originalOrCopy()
    , _PTSDTSFlags()
    , _ESCRFlags()
    , _ESRateFlag()
    , _DSMTrickModeFlag()
    , _additionalCopyInfoFlag()
    , _PES_CRCFlag()
    , _PESExtensionFlag()
    , _PESHeaderDataLength()
    , _decoder(decoder)
{
}

bool PESPacket::Parse(ByteIterator start, ByteIterator end, bool hasStartIndicator)
{
    Tools::BitBuffer buffer(start, end);
    if (hasStartIndicator)
    {
        _startCode = buffer.ReadBits<uint32_t>(24);
        if (_startCode != StartCodeValue)
        {
            return false;
        }
        _streamID = static_cast<StreamID >(buffer.ReadBits<uint8_t>(8));
        _packetLength = buffer.ReadBits<uint16_t>(16);
        if (StreamHasHeader())
        {
            if (buffer.ReadBits<uint8_t>(2) != 0x00000002)  // Must be 10
                return false;
            _PESScramblingControl = static_cast<ScrambingControl>(buffer.ReadBits<uint8_t>(2));
            _PESPriority = buffer.ReadBit();
            _dataAlignmentIndicator = buffer.ReadBit();
            _copyright = buffer.ReadBit();
            _originalOrCopy = buffer.ReadBit();
            _PTSDTSFlags = static_cast<PTSDTSFlags>(buffer.ReadBits<uint8_t>(2));
            _ESCRFlags = buffer.ReadBit();
            _ESRateFlag = buffer.ReadBit();
            _DSMTrickModeFlag = buffer.ReadBit();
            _additionalCopyInfoFlag = buffer.ReadBit();
            _PES_CRCFlag = buffer.ReadBit();
            _PESExtensionFlag = buffer.ReadBit();
            _PESHeaderDataLength = buffer.ReadBits<uint8_t>(8);
            if ((_PTSDTSFlags == PTSDTSFlags::PTSOnly) ||
                (_PTSDTSFlags == PTSDTSFlags::PTSAndDTS))
            {
                if (buffer.ReadBits<uint8_t>(4) != static_cast<uint8_t>(_PTSDTSFlags))  // Must be equal to flags
                    return false;
                _PTS = buffer.ReadBits<uint64_t>(3) << 30;
                if (!buffer.ReadBit())                  // Marker bit = 1
                    return false;
                _PTS |= buffer.ReadBits<uint64_t>(15) << 15;
                if (!buffer.ReadBit())                  // Marker bit = 1
                    return false;
                _PTS |= buffer.ReadBits<uint64_t>(15) << 0;
                if (!buffer.ReadBit())                  // Marker bit = 1
                    return false;
                if (_PTSDTSFlags == PTSDTSFlags::PTSAndDTS)
                {
                    if (buffer.ReadBits<uint8_t>(4) != 0x01)  // Must be 0001
                        return false;
                    _DTS = buffer.ReadBits<uint64_t>(3) << 30;
                    if (!buffer.ReadBit())                  // Marker bit = 1
                        return false;
                    _DTS |= buffer.ReadBits<uint64_t>(15) << 15;
                    if (!buffer.ReadBit())                  // Marker bit = 1
                        return false;
                    _DTS |= buffer.ReadBits<uint64_t>(15) << 0;
                    if (!buffer.ReadBit())                  // Marker bit = 1
                        return false;
                }
            }
            if (_ESCRFlags)
            {
                throw std::logic_error("Not implemented");
            }
            if (_ESRateFlag)
            {
                throw std::logic_error("Not implemented");
            }
            if (_DSMTrickModeFlag)
            {
                throw std::logic_error("Not implemented");
            }
            if (_additionalCopyInfoFlag)
            {
                throw std::logic_error("Not implemented");
            }
            if (_PES_CRCFlag)
            {
                throw std::logic_error("Not implemented");
            }
            if (_PESExtensionFlag)
            {
                throw std::logic_error("Not implemented");
            }
        }
    }
    if (IsPaddingStream())
    {
        for (size_t i = 0; i < _packetLength; ++i)
            buffer.ReadBits<uint8_t>(8); // Simply consume padding bytes
    }
    else
    {
        if (_decoder)
        {
            _decoder->FeedData(buffer.Current(), buffer.BytesLeft());
        }
    }
    return IsValid();
}

bool PESPacket::IsValid() const
{
    return (_startCode == StartCodeValue) &&
        (_PTSDTSFlags != PTSDTSFlags::Forbidden);
}

bool PESPacket::IsAudioStream() const
{
    return (static_cast<uint8_t>(_streamID) & AudioStreamMask) == AudioStreamID;
}

bool PESPacket::IsVideoStream() const
{
    return (static_cast<uint8_t>(_streamID) & VideoStreamMask) == VideoStreamID;
}

bool PESPacket::IsPaddingStream() const
{
    return _streamID == StreamID::PaddingStream;
}

bool PESPacket::StreamHasHeader() const
{
    return (_streamID != StreamID::ProgramStreamMap) &&
           (_streamID != StreamID::PaddingStream) &&
           (_streamID != StreamID::PrivateStream2) &&
           (_streamID != StreamID::ECMStream) &&
           (_streamID != StreamID::EMMStream) &&
           (_streamID != StreamID::ProgramStreamDirectory) &&
           (_streamID != StreamID::DSMCCStream) &&
           (_streamID != StreamID::ITU_T_H222_1_E_Stream);
}

void PESPacket::DisplayContents() const
{
    Tools::DefaultConsole() << endl << "PMT" << endl << endl;
    Tools::DefaultConsole() << "StartCode:        " << Tools::Serialize(_startCode, 16) << endl;
    Tools::DefaultConsole() << "Stream ID:        " << Tools::Serialize(static_cast<uint8_t>(_streamID), 16) << endl;
    Tools::DefaultConsole() << "Audio stream:     " << (IsAudioStream() ? "Yes" : "No") << endl;
    Tools::DefaultConsole() << "Video stream:     " << (IsVideoStream() ? "Yes" : "No") << endl;
    Tools::DefaultConsole() << "Packet length:    " << Tools::Serialize(_packetLength, 10) << endl;
}
