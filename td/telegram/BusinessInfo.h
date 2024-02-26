//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2024
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/BusinessWorkHours.h"
#include "td/telegram/DialogLocation.h"
#include "td/telegram/td_api.h"

#include "td/utils/common.h"

namespace td {

class Td;

class BusinessInfo {
 public:
  td_api::object_ptr<td_api::businessInfo> get_business_info_object() const;

  bool is_empty() const;

  static bool set_location(unique_ptr<BusinessInfo> &business_info, DialogLocation &&location);

  static bool set_work_hours(unique_ptr<BusinessInfo> &business_info, BusinessWorkHours &&work_hours);

  template <class StorerT>
  void store(StorerT &storer) const;

  template <class ParserT>
  void parse(ParserT &parser);

 private:
  static bool is_empty_location(const DialogLocation &location);

  DialogLocation location_;
  BusinessWorkHours work_hours_;
};

}  // namespace td
