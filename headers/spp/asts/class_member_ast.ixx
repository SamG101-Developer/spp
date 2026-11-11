module;
#include <spp/macros.hpp>

export module spp.asts.class_member_ast;
import spp.asts.ast;

use_common(ClassMemberAst);

/// Tag-based base class for all members of a class. Only
/// ClassAttributeAst uses it for now, but it allows for simple
/// expansion in the future.
SPP_EXP_CLS struct spp::asts::ClassMemberAst {
  ClassMemberAst();

  virtual ~ClassMemberAst();
};
