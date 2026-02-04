#include "multi_zipfian_generator.h"

namespace ycsbc
{
    AutoHotZoneSelector::AutoHotZoneSelector(uint64_t min_key, uint64_t max_key,
                                             double read_center_pos,
                                             double write_center_pos,
                                             double read_theta,
                                             double write_theta)
        : min_key_(min_key),
          max_key_(max_key),
          total_keys_(max_key - min_key + 1),
          read_theta_(read_theta),
          write_theta_(write_theta),
          read_center_pos_(read_center_pos),
          write_center_pos_(write_center_pos),
          read_center_(CalculateCenter(total_keys_, read_center_pos)),
          write_center_(CalculateCenter(total_keys_, write_center_pos)),
          read_zipfs_(min_key_, total_keys_ - 1, read_theta),
          write_zipfs_(min_key_, total_keys_ - 1, write_theta)
    {
        Next(OperationType::kRead);
        Next(OperationType::kWrite);
    }
    AutoHotZoneSelector::~AutoHotZoneSelector()
    {
        // delete read_zipfs_;
        // delete write_zipfs_;
    }
    uint64_t AutoHotZoneSelector::Next(OperationType op_type)
    {
        if (op_type == OperationType::kRead)
        {
            uint64_t offset = read_zipfs_.Next();
            uint64_t mapped_offset = (read_center_ + offset - min_key_) % total_keys_;
            last_read_key_ = min_key_ + mapped_offset;
            // std::cout<<"Read: "<<read_center_pos_<<" "<<write_center_pos_<<" "<<read_theta_<<" "<<write_theta_<<" "<<last_read_key_<<std::endl;
            return last_read_key_;
        }
        else
        {
            uint64_t offset = write_zipfs_.Next();
            uint64_t mapped_offset = (write_center_ + offset - min_key_) % total_keys_;
            last_write_key_ = min_key_ + mapped_offset;
            // std::cout<<"Update: "<<read_center_pos_<<" "<<write_center_pos_<<" "<<read_theta_<<" "<<write_theta_<<" "<<last_write_key_<<std::endl;
            return last_write_key_;
        }
    }
    uint64_t AutoHotZoneSelector::Next()
    {
        return Next(OperationType::kRead);
    }
    uint64_t AutoHotZoneSelector::Last(OperationType op_type)
    {
        return (op_type == OperationType::kRead) ? last_read_key_ : last_write_key_;
    }
    uint64_t AutoHotZoneSelector::Last()
    {
        return Next(OperationType::kRead);
    }
    uint64_t AutoHotZoneSelector::CalculateCenter(uint64_t total_keys, double pos) const
    {
        if (total_keys == 0)
        {
            return 0;
        }
        uint64_t center = min_key_ + static_cast<uint64_t>(pos * (total_keys - 1));
        return center;
    }
}