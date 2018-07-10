#pragma once

#include "glib_equals_string.hxx"

#include <vector>

template <typename T> class Vector_State
{
public:
  std::string name;
  bool inside = false;
  T element_state;
  std::vector<decltype(element_state.value)> value;

  Vector_State(const std::vector<std::string> &names,
               const size_t &offset)
      : name(names.at(offset)), element_state(names, offset + 1)
  {}
  Vector_State(const std::initializer_list<std::string> &names)
      : Vector_State(names, 0)
  {}

  bool on_start_element(const Glib::ustring &element_name,
                        const xmlpp::SaxParser::AttributeList &attributes)
  {
    if(inside)
      {
        if(!element_state.on_start_element(element_name, attributes))
          {
            throw std::runtime_error("Invalid input file.  Expected '"
                                     + element_state.name + "' inside '" + name
                                     + "', but found '" + element_name + "'");
          }
      }
    else if(glib_equals_string(element_name, name))
      {
        inside = true;
        value.clear();
      }
    return inside;
  }

  bool on_end_element(const Glib::ustring &element_name)
  {
    bool result(false);
    if(inside)
      {
        if(element_state.on_end_element(element_name))
          {
            value.emplace_back();
            std::swap(value.back(),element_state.value);
          }
        else if(glib_equals_string(element_name, name))
          {
            inside = false;
            result = true;
          }
      }
    return result;
  }

  bool on_characters(const Glib::ustring &characters)
  {
    if(inside)
      {
        element_state.on_characters(characters);
      }
    return inside;
  }
};