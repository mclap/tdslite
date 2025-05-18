#pragma once

#include "tds_frames.h"
#include "frame_token_row.h"

namespace tds
{

struct frame_response
{
    std::deque<frame_token_error> errors;
    bool auth_success;
    frame_token_colmetadata columns_info;
    std::deque<frame_token_row> rows;

    frame_response();

    bool decode(buffer& input);
};

} // namespace tds
