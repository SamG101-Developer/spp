module;
#include <spp/macros.hpp>

export module spp.analyse.utils.instantiation_queue;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct FunctionPrototypeAst;
}

/**
 * The record of which function templates have had a generic substitution registered against them that has not been
 * processed yet.
 *
 * @n
 * Instantiations are discovered lazily, by analysing a call: resolving "1 + 2" is what mints
 * "SizedInteger[32, true]::add", and analysing @e that body is what mints "intrinsics::add[T=S32]". The set therefore
 * only closes over itself, and never in the order the modules happen to be walked in - a walk that reaches
 * "intrinsics.spp" before "sized_integer.spp" never sees the second instantiation at all. Recording the templates as
 * they are instantiated, and draining the record to a fixed point, is what makes the set complete before anything
 * downstream depends on it.
 *
 * @n
 * The record holds templates rather than the substitutions themselves. A substitution is registered before the
 * prototype that fills it has been built (see @c CreateGenericFunScope ), so an entry recorded at registration time
 * would name a slot that is still empty; re-reading the template's list at drain time is what guarantees the
 * substitution is whole by the time it is looked at. Each substitution carries its own processed flag, so a template
 * that comes up more than once costs nothing the second time.
 */
namespace spp::analyse::utils::instantiation_queue {
  /**
   * Record that @p fn_template has a substitution needing processing. Queueing a template already waiting is a no-op.
   * @param fn_template The template prototype a substitution was just registered against.
   */
  SPP_EXP_FUN auto Enqueue(
    asts::FunctionPrototypeAst *fn_template)
    -> void;

  /**
   * Take the next template waiting to be processed, in the order they were recorded - which keeps a failure inside an
   * instantiation reported against the call that caused it rather than against an unrelated later one.
   * @return The template, or @c nullptr when nothing is waiting.
   */
  SPP_EXP_FUN auto Pop()
    -> asts::FunctionPrototypeAst*;

  /**
   * Drop everything waiting. Called between compilations, because the record outlives any one of them.
   */
  SPP_EXP_FUN auto Clear()
    -> void;
}
