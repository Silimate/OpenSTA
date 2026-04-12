// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#pragma once

#include <string>
#include <memory>
#include <stack>
#include <set>
#include "StringSeq.hh"
#include "Error.hh"
#include "Sta.hh"
#include "Property.hh"
#include "PatternMatch.hh"

namespace sta {

using std::string;

class FilterError : public Exception
{
public:
  explicit FilterError(std::string_view error);
  virtual ~FilterError() noexcept {}
  virtual const char *what() const noexcept;

private:
  std::string error_;
};

class FilterExpr {
public:
    struct Token {
        enum class Kind {
            skip = 0,
            predicate,
            op_lparen,
            op_rparen,
            op_or,
            op_and,
            op_inv,
            defined,
            undefined
        };
        
        Token(std::string text, Kind kind);
        
        std::string text;
        Kind kind;
    };
    
    struct PredicateToken : public Token {
      PredicateToken(std::string property, std::string op, std::string arg);
      
      std::string property;
      std::string op;
      std::string arg;
    };
    
    FilterExpr(std::string expression);
    
    std::vector<std::shared_ptr<Token>> postfix(bool sta_boolean_props_as_int);
private:
    std::vector<std::shared_ptr<Token>> lex(bool sta_boolean_props_as_int);
    std::vector<std::shared_ptr<Token>> shuntingYard(std::vector<std::shared_ptr<Token>>& infix);
    
    std::string raw_;
};

template <typename T> std::set<T*>
process_predicate(const char *property,
	       const char *op,
	       const char *pattern,
	       std::set<T*> &all)
{
  auto filtered_objects = std::set<T*>();
  bool exact_match = stringEq(op, "==");
  bool pattern_match = stringEq(op, "=~");
  bool not_match = stringEq(op, "!=");
  bool not_pattern_match = stringEq(op, "!~");
  for (T *object : all) {
    PropertyValue value = Sta::sta()->properties().getProperty(object, property);
    std::string prop_str = value.to_string(Sta::sta()->network());
    const char *prop = prop_str.c_str();
    if (prop &&
        ((exact_match && stringEq(prop, pattern))
          || (not_match && !stringEq(prop, pattern))
          || (pattern_match && patternMatch(pattern, prop))
          || (not_pattern_match && !patternMatch(pattern, prop))))
      filtered_objects.insert(object);
  }
  return filtered_objects;
}

template <typename T> Vector<T*>
filter_objects(const char *filter_expression,
	       Vector<T*> *objects,
         bool sta_boolean_props_as_int
        ) {
  Vector<T*> result;
  if (objects) {
    auto all = std::set<T*>();
    for (auto object: *objects) {
      all.insert(object);
    }
    auto postfix = sta::FilterExpr(filter_expression).postfix(sta_boolean_props_as_int);
    std::stack<std::set<T*>> eval_stack;
    for (auto &pToken: postfix) {
      if (pToken->kind == sta::FilterExpr::Token::Kind::op_or) {
        if (eval_stack.size() < 2) {
          throw sta::FilterError("attempted to run a logical or on less than two predicates");
        }
        auto arg0 = eval_stack.top();
        eval_stack.pop();
        auto arg1 = eval_stack.top();
        eval_stack.pop();
        auto union_result = std::set<T*>();
        std::set_union(
          arg0.cbegin(), arg0.cend(),
          arg1.cbegin(), arg1.cend(),
          std::inserter(union_result, union_result.begin())
        );
        eval_stack.push(union_result);
      } else if (pToken->kind == sta::FilterExpr::Token::Kind::op_and) {
        if (eval_stack.size() < 2) {
          throw sta::FilterError("attempted to run a logical and on less than two predicates");
        }
        auto arg0 = eval_stack.top();
        eval_stack.pop();
        auto arg1 = eval_stack.top();
        eval_stack.pop();
        auto intersection_result = std::set<T*>();
        std::set_intersection(
          arg0.cbegin(), arg0.cend(),
          arg1.cbegin(), arg1.cend(),
          std::inserter(intersection_result, intersection_result.begin())
        );
        eval_stack.push(intersection_result);
      } else if (pToken->kind == sta::FilterExpr::Token::Kind::op_inv) {
        if (eval_stack.size() < 1) {
          throw sta::FilterError("attempted to run an inversion on no predicates");
        }
        auto arg0 = eval_stack.top();
        eval_stack.pop();
        
        auto difference_result = std::set<T*>();
        std::set_difference(
          all.cbegin(), all.cend(),
          arg0.cbegin(), arg0.cend(),
          std::inserter(difference_result, difference_result.begin())
        );
        eval_stack.push(difference_result);
      } else if (pToken->kind == sta::FilterExpr::Token::Kind::defined ||
                 pToken->kind == sta::FilterExpr::Token::Kind::undefined) {
        bool should_be_defined = (pToken->kind == sta::FilterExpr::Token::Kind::defined);
        auto result = std::set<T*>();
        for (auto object : all) {
          PropertyValue value = Sta::sta()->properties().getProperty(object, pToken->text);
          bool is_defined = false;
          switch (value.type()) {
            case PropertyValue::Type::type_float:
              is_defined = value.floatValue() != 0;
              break;
            case PropertyValue::Type::type_bool:
              is_defined = value.boolValue();
              break;
            case PropertyValue::Type::type_string:
            case PropertyValue::Type::type_liberty_library:
            case PropertyValue::Type::type_liberty_cell:
            case PropertyValue::Type::type_liberty_port:
            case PropertyValue::Type::type_library:
            case PropertyValue::Type::type_cell:
            case PropertyValue::Type::type_port:
            case PropertyValue::Type::type_instance:
            case PropertyValue::Type::type_pin:
            case PropertyValue::Type::type_net:
            case PropertyValue::Type::type_clk:
              is_defined = value.to_string(Sta::sta()->network()) != "";
              break;
            case PropertyValue::Type::type_none:
              is_defined = false;
              break;
            case PropertyValue::Type::type_pins:
              is_defined = value.pins()->size() > 0;
              break;
            case PropertyValue::Type::type_clks:
              is_defined = value.clocks()->size() > 0;
              break;
            case PropertyValue::Type::type_paths:
              is_defined = value.paths()->size() > 0;
              break;
            case PropertyValue::Type::type_pwr_activity:
              is_defined = value.pwrActivity().isSet();
              break;
          }
          if (is_defined == should_be_defined) {
            result.insert(object);
          }
        }
        eval_stack.push(result);
      } else if (pToken->kind == sta::FilterExpr::Token::Kind::predicate) {
        auto predicate_token = std::static_pointer_cast<sta::FilterExpr::PredicateToken>(pToken);
        auto predicate_result = process_predicate<T>(predicate_token->property.c_str(), predicate_token->op.c_str(), predicate_token->arg.c_str(), all);
        eval_stack.push(predicate_result);
      }
    }
    if (eval_stack.size() == 0) {
      throw sta::FilterError("filter expression is empty");
    }
    if (eval_stack.size() > 1) {
      throw sta::FilterError("filter expression evaluated to multiple sets");
    }
    auto result_set = eval_stack.top();
    result.resize(result_set.size());
    std::copy(result_set.begin(), result_set.end(), result.begin());
    
    // Maintain pre-filter ordering
    std::map<T*, int> objects_i;
    for (int i = 0; i < objects->size(); ++i)
      objects_i[objects->at(i)] = i;

    std::sort(result.begin(), result.end(),
      [&](T* a, T* b) {
          return objects_i[a] < objects_i[b];
    });
  }
  return result;
}


} // namespace
