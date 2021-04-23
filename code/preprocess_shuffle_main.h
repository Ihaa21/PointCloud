#pragma once

//
// NOTE: Float Points
//

struct pc_point
{
    v3 Pos;
    u32 Color;
};

struct sorted_point
{
    u64 Key;
    pc_point Point;
};

struct sorted_group
{
    u32 StartId;
    u32 RandValue;
};
