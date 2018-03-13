// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.


#include "expression.h"
#include "../exception.h"
#include "../internal.h"
#include <avs/win.h>
#include <cassert>
#include <vector>


class BreakStmtException
{
};

AVSValue ExpRootBlock::Evaluate(IScriptEnvironment* env) 
{
  AVSValue retval;

  try {
    retval = exp->Evaluate(env);
  }
  catch (const ReturnExprException &e) {
    retval = e.value;
  }

  return retval;
}

AVSValue ExpSequence::Evaluate(IScriptEnvironment* env) 
{
    AVSValue last = a->Evaluate(env);
    if (last.IsClip()) env->SetVar("last", last);
    return b->Evaluate(env);
}

AVSValue ExpExceptionTranslator::Evaluate(IScriptEnvironment* env) 
{
  try {
    SehGuard seh_guard;
    return exp->Evaluate(env);
  }
  catch (const IScriptEnvironment::NotFound&) {
    throw;
  }
  catch (const AvisynthError&) {
    throw;
  }
  catch (const BreakStmtException&) {
    throw;
  }
  catch (const ReturnExprException&) {
    throw;
  }
  catch (const SehException &seh) {
    if (seh.m_msg)
      env->ThrowError(seh.m_msg);
	  else
      env->ThrowError("Evaluate: System exception - 0x%x", seh.m_code);
  }
  catch (...) {
    env->ThrowError("Evaluate: Unhandled C++ exception!");
  }
  return 0;
}


AVSValue ExpTryCatch::Evaluate(IScriptEnvironment* env) 
{
  AVSValue result;
  try {
    return ExpExceptionTranslator::Evaluate(env);
  }
  catch (const AvisynthError &ae) {
    env->SetVar(id, ae.msg);
    return catch_block->Evaluate(env);
  }
}


AVSValue ExpLine::Evaluate(IScriptEnvironment* env) 
{
  try {
    return ExpExceptionTranslator::Evaluate(env);
  }
  catch (const AvisynthError &ae) {
    env->ThrowError("%s\n(%s, line %d)", ae.msg, filename, line);
  }
  return 0;
}

AVSValue ExpBlockConditional::Evaluate(IScriptEnvironment* env) 
{
  AVSValue result;
  IScriptEnvironment2 *env2 = static_cast<IScriptEnvironment2*>(env);
  env2->GetVar("last", &result);

  AVSValue cond = If->Evaluate(env);
  if (!cond.IsBool())
    env->ThrowError("if: condition must be boolean (true/false)");
  if (cond.AsBool())
  {
    if (Then) // note: "Then" can also be NULL if its block is empty
      result = Then->Evaluate(env);
  }
  else if (Else) // note: "Else" can also be NULL if its block is empty
    result = Else->Evaluate(env);

  if (result.IsClip())
    env->SetVar("last", result);

  return result;
}

AVSValue ExpWhileLoop::Evaluate(IScriptEnvironment* env) 
{
  AVSValue result;
  IScriptEnvironment2 *env2 = static_cast<IScriptEnvironment2*>(env);
  env2->GetVar("last", &result);

  AVSValue cond;
  do {
    cond = condition->Evaluate(env);
    if (!cond.IsBool())
      env->ThrowError("while: condition must be boolean (true/false)");

    if (!cond.AsBool())
      break;

    if (body)
    {
      try
      {
        result = body->Evaluate(env);
        if (result.IsClip())
          env->SetVar("last", result);
      }
      catch(const BreakStmtException&)
      {
        break;
      }
    }
  }
  while (true);
  
  return result;
}

AVSValue ExpForLoop::Evaluate(IScriptEnvironment* env) 
{
  const AVSValue initVal = init->Evaluate(env),
                 limitVal = limit->Evaluate(env),
                 stepVal = step->Evaluate(env);

  if (!initVal.IsInt())
    env->ThrowError("for: initial value must be int");
  if (!limitVal.IsInt())
    env->ThrowError("for: final value must be int");
  if (!stepVal.IsInt())
    env->ThrowError("for: step value must be int");
  if (stepVal.AsInt() == 0)
    env->ThrowError("for: step value must be non-zero");

  const int iLimit = limitVal.AsInt(), iStep = stepVal.AsInt();
  int i = initVal.AsInt();

  AVSValue result;
  IScriptEnvironment2 *env2 = static_cast<IScriptEnvironment2*>(env);
  env2->GetVar("last", &result);

  env->SetVar(id, initVal);
  while (iStep > 0 ? i <= iLimit : i >= iLimit)
  {
    if (body)
    {
      try
      {
        result = body->Evaluate(env);
        if (result.IsClip())
          env->SetVar("last", result);
      }
      catch(const BreakStmtException&)
      {
        break;
      }
    }

    AVSValue idVal = env->GetVar(id); // may have been updated in body
    if (!idVal.IsInt())
      env->ThrowError("for: loop variable '%s' has been assigned a non-int value", id);
    i = idVal.AsInt() + iStep;
    env->SetVar(id, i);
  }  
  return result;  // overall result is that of final body evaluation (if any)
}

AVSValue ExpBreak::Evaluate(IScriptEnvironment* env) 
{
  throw BreakStmtException();
}

AVSValue ExpConditional::Evaluate(IScriptEnvironment* env) 
{
  AVSValue cond = If->Evaluate(env);
  if (!cond.IsBool())
    env->ThrowError("Evaluate: left of `?' must be boolean (true/false)");
  return (cond.AsBool() ? Then : Else)->Evaluate(env);
}

AVSValue ExpReturn::Evaluate(IScriptEnvironment* env)
{
	ReturnExprException ret;
	ret.value = value->Evaluate(env);
	throw ret;
}



/**** Operators ****/

AVSValue ExpOr::Evaluate(IScriptEnvironment* env) 
{
  AVSValue x = a->Evaluate(env);
  if (!x.IsBool())
    env->ThrowError("Evaluate: left operand of || must be boolean (true/false)");
  if (x.AsBool())
    return x;
  AVSValue y = b->Evaluate(env);
  if (!y.IsBool())
    env->ThrowError("Evaluate: right operand of || must be boolean (true/false)");
  return y;
}


AVSValue ExpAnd::Evaluate(IScriptEnvironment* env) 
{
  AVSValue x = a->Evaluate(env);
  if (!x.IsBool())
    env->ThrowError("Evaluate: left operand of && must be boolean (true/false)");
  if (!x.AsBool())
    return x;
  AVSValue y = b->Evaluate(env);
  if (!y.IsBool())
    env->ThrowError("Evaluate: right operand of && must be boolean (true/false)");
  return y;
}


AVSValue ExpEqual::Evaluate(IScriptEnvironment* env) 
{
  AVSValue x = a->Evaluate(env);
  AVSValue y = b->Evaluate(env);
  if (x.IsBool() && y.IsBool()) {
    return x.AsBool() == y.AsBool();
  }
  else if (x.IsInt() && y.IsInt()) {
    return x.AsInt() == y.AsInt();
  }
  else if (x.IsFloat() && y.IsFloat()) {
    return x.AsFloat() == y.AsFloat();
  }
  else if (x.IsClip() && y.IsClip()) {
    return x.AsClip() == y.AsClip();
  }
  else if (x.IsString() && y.IsString()) {
    return !lstrcmpi(x.AsString(), y.AsString());
  }
  else {
    env->ThrowError("Evaluate: operands of `==' and `!=' must be comparable");
    return 0;
  }
}


AVSValue ExpLess::Evaluate(IScriptEnvironment* env)
{
  AVSValue x = a->Evaluate(env);
  AVSValue y = b->Evaluate(env);
  if (x.IsInt() && y.IsInt()) {
    return x.AsInt() < y.AsInt();
  }
  else if (x.IsFloat() && y.IsFloat()) {
    return x.AsFloat() < y.AsFloat();
  }
  else if (x.IsString() && y.IsString()) {
    return _stricmp(x.AsString(),y.AsString()) < 0 ? true : false;
  }
  else {
    env->ThrowError("Evaluate: operands of `<' and friends must be string or numeric");
    return 0;
  }
}

AVSValue ExpPlus::Evaluate(IScriptEnvironment* env)
{
  AVSValue x = a->Evaluate(env);
  AVSValue y = b->Evaluate(env);
  if (x.IsClip() && y.IsClip())
    return new_Splice(x.AsClip(), y.AsClip(), false, env);    // UnalignedSplice
  else if (x.IsInt() && y.IsInt())
    return x.AsInt() + y.AsInt();
  else if (x.IsFloat() && y.IsFloat())
    return x.AsFloat() + y.AsFloat();
  else if (x.IsString() && y.IsString())
    return env->Sprintf("%s%s", x.AsString(), y.AsString());
  else {
    env->ThrowError("Evaluate: operands of `+' must both be numbers, strings, or clips");
    return 0;
  }
}


AVSValue ExpDoublePlus::Evaluate(IScriptEnvironment* env)
{
  AVSValue x = a->Evaluate(env);
  AVSValue y = b->Evaluate(env);
  if (x.IsClip() && y.IsClip())
    return new_Splice(x.AsClip(), y.AsClip(), true, env);    // AlignedSplice
  else {
    env->ThrowError("Evaluate: operands of `++' must be clips");
    return 0;
  }
}


AVSValue ExpMinus::Evaluate(IScriptEnvironment* env) 
{
  AVSValue x = a->Evaluate(env);
  AVSValue y = b->Evaluate(env);
  if (x.IsInt() && y.IsInt())
    return x.AsInt() - y.AsInt();
  else if (x.IsFloat() && y.IsFloat())
    return x.AsFloat() - y.AsFloat();
  else {
    env->ThrowError("Evaluate: operands of `-' must be numeric");
    return 0;
  }
}


AVSValue ExpMult::Evaluate(IScriptEnvironment* env) 
{
  AVSValue x = a->Evaluate(env);
  AVSValue y = b->Evaluate(env);
  if (x.IsInt() && y.IsInt())
    return x.AsInt() * y.AsInt();
  else if (x.IsFloat() && y.IsFloat())
    return x.AsFloat() * y.AsFloat();
  else {
    env->ThrowError("Evaluate: operands of `*' must be numeric");
    return 0;
  }
}


AVSValue ExpDiv::Evaluate(IScriptEnvironment* env)
{
  AVSValue x = a->Evaluate(env);
  AVSValue y = b->Evaluate(env);
  if (x.IsInt() && y.IsInt()) {
    if (y.AsInt() == 0)
      env->ThrowError("Evaluate: division by zero");
    return x.AsInt() / y.AsInt();
  }
  else if (x.IsFloat() && y.IsFloat())
    return x.AsFloat() / y.AsFloat();
  else {
    env->ThrowError("Evaluate: operands of `/' must be numeric");
    return 0;
  }
}


AVSValue ExpMod::Evaluate(IScriptEnvironment* env) 
{
  AVSValue x = a->Evaluate(env);
  AVSValue y = b->Evaluate(env);
  if (x.IsInt() && y.IsInt()) {
    if (y.AsInt() == 0)
      env->ThrowError("Evaluate: division by zero");
    return x.AsInt() % y.AsInt();
  }
  else {
    env->ThrowError("Evaluate: operands of `%%' must be integers");
    return 0;
  }
}


AVSValue ExpNegate::Evaluate(IScriptEnvironment* env)
{
  AVSValue x = e->Evaluate(env);
  if (x.IsInt())
    return -x.AsInt();
  else if (x.IsFloat())
    return -x.AsFloat();
  else {
    env->ThrowError("Evaluate: unary minus can only by used with numbers");
    return 0;
  }
}


AVSValue ExpNot::Evaluate(IScriptEnvironment* env)
{
  AVSValue x = e->Evaluate(env);
  if (x.IsBool())
    return !x.AsBool();
  else {
    env->ThrowError("Evaluate: operand of `!' must be boolean (true/false)");
    return 0;
  }
}


AVSValue ExpVariableReference::Evaluate(IScriptEnvironment* env) 
{
  AVSValue result;
  IScriptEnvironment2 *env2 = static_cast<IScriptEnvironment2*>(env);

  // first look for a genuine variable
  // Don't add a cache to this one, it's a Var
  if (env2->GetVar(name, &result)) {
    return result;
  }
  else {
    // Swap order to match ::Call below -- Gavino Jan 2010

    // next look for an argless function
    if (!env2->Invoke(&result, name, AVSValue(0,0)))
    {
      // finally look for a single-arg function taking implicit "last"
      AVSValue last;
      if (!env2->GetVar("last", &last) || !env2->Invoke(&result, name, last))
      {
        env->ThrowError("I don't know what '%s' means.", name);
        return 0;
      }
    }
  }

  return result;
}


AVSValue ExpAssignment::Evaluate(IScriptEnvironment* env)
{
  env->SetVar(lhs, rhs->Evaluate(env));
  return AVSValue();
}


AVSValue ExpGlobalAssignment::Evaluate(IScriptEnvironment* env) 
{
  env->SetGlobalVar(lhs, rhs->Evaluate(env));
  return AVSValue();
}


ExpFunctionCall::ExpFunctionCall( const char* _name, PExpression* _arg_exprs,
                   const char** _arg_expr_names, int _arg_expr_count, bool _oop_notation )
  : name(_name), arg_expr_count(_arg_expr_count), oop_notation(_oop_notation)
{
  arg_exprs = new PExpression[arg_expr_count];
  // arg_expr_names has an extra elt at the beginning, for implicit "last"
  arg_expr_names = new const char*[arg_expr_count+1];
  arg_expr_names[0] = 0;
  for (int i=0; i<arg_expr_count; ++i) {
    arg_exprs[i] = _arg_exprs[i];
    arg_expr_names[i+1] = _arg_expr_names[i];
  }
}

ExpFunctionCall::~ExpFunctionCall(void)
{
  delete[] arg_exprs;
  delete[] arg_expr_names;
}

AVSValue ExpFunctionCall::Evaluate(IScriptEnvironment* env)
{
  AVSValue result;
  IScriptEnvironment2 *env2 = static_cast<IScriptEnvironment2*>(env);

  std::vector<AVSValue> args(arg_expr_count+1, AVSValue());
  for (int a=0; a<arg_expr_count; ++a)
    args[a+1] = arg_exprs[a]->Evaluate(env);

  // first try without implicit "last"
  try
  { // Invoke can always throw by calling a constructor of a filter that throws
    if (env2->Invoke(&result, name, AVSValue(args.data()+1, arg_expr_count), arg_expr_names+1))
      return result;
  } catch(const IScriptEnvironment::NotFound&){}

  // if that fails, try with implicit "last" (except when OOP notation was used)
  if (!oop_notation) 
  {
    try
    {
      if (env2->GetVar("last", args.data()) && env2->Invoke(&result, name, AVSValue(args.data(), arg_expr_count+1), arg_expr_names))
        return result;
    } catch(const IScriptEnvironment::NotFound&){}
  }

  env->ThrowError(env->FunctionExists(name) ?
    "Script error: Invalid arguments to function '%s'." :
  "Script error: There is no function named '%s'.", name);

  assert(0);  // we should never get here
  return 0;
}
