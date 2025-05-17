#include "tdslite/tds_frames.h"
#include "tdslite/frame_token_row.h"

namespace tds
{

bool frame_token_row::decode(const frame_token_colmetadata& meta, buffer& input)
{
    std::vector<unsigned char> bitmap;

    if (isNBC())
    {
        // NullBitmap
        bitmap.resize((meta.info.size()+7) >> 3);
        input.fetch(&bitmap[0], bitmap.size());
    }

    frame_token_colmetadata::info_type::const_iterator iter =
        meta.info.begin();
    int n = 0;
    for ( ; iter != meta.info.end(); ++ iter, ++n)
    {
        const bool isNull = isNBC() &&
            (0 != (bitmap[n>>3] & (1 << (n & 0x07))));

        column_data elm;
        if (isNull)
            elm.isNull = isNull;
        else
        if (!elm.decode(*iter, input))
            return false;

        data.push_back(elm);
    }

    return true;
}

} // namespace tds
