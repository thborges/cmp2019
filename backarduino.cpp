#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/InlineAsm.h>

extern "C" {
	#include "header.h"
}

#ifdef ENABLE_ARDUINO

using namespace llvm;

extern LLVMContext ctx;
extern Module *module;

Function *analogWrite = NULL;
Function *analogRead = NULL;
Function *delay = NULL;
Function *delayMicroseconds = NULL;
Function *setup = NULL;
Function *init = NULL;
Function *print = NULL;

void declare_auxiliary_arduino_funcs() {
	std::vector<Type*> arg_types;
	FunctionType *ftype;

	#ifdef ENABLE_ARDUINO
	// analogRead
	arg_types.clear();
	arg_types.push_back(Type::getInt8Ty(ctx));
	ftype = FunctionType::get(Type::getInt16Ty(ctx), arg_types, false);
	analogRead = Function::Create(ftype, Function::ExternalLinkage, "analogRead", module);
	analogRead->setCallingConv(CallingConv::C);

	// analogWrite or digitalWrite
	arg_types.clear();
	arg_types.push_back(Type::getInt8Ty(ctx));
	arg_types.push_back(Type::getInt16Ty(ctx));
	ftype = FunctionType::get(Type::getVoidTy(ctx), arg_types, false);
	analogWrite = Function::Create(ftype, Function::ExternalLinkage, "analogWrite", module);

	// digitalWrite instead
	/*arg_types.clear();
	arg_types.push_back(Type::getInt8Ty(ctx));
	arg_types.push_back(Type::getInt8Ty(ctx));
	ftype = FunctionType::get(Type::getVoidTy(ctx), arg_types, false);
	analogWrite = Function::Create(ftype, Function::ExternalLinkage, "digitalWrite", module);
	*/

	analogWrite->setCallingConv(CallingConv::C);

	// delay 
	arg_types.clear();
	arg_types.push_back(Type::getInt32Ty(ctx));
	ftype = FunctionType::get(Type::getVoidTy(ctx),
		ArrayRef<Type*>(arg_types), false);
	delay = Function::Create(ftype, Function::ExternalLinkage, "delay", module);
	delay->setCallingConv(CallingConv::C);


	// delayMicroseconds
	arg_types.clear();
	arg_types.push_back(Type::getInt32Ty(ctx));
	ftype = FunctionType::get(Type::getVoidTy(ctx),
		ArrayRef<Type*>(arg_types), false);
	delayMicroseconds = Function::Create(ftype, Function::ExternalLinkage, "delayMicroseconds", module);
	delayMicroseconds->setCallingConv(CallingConv::C);
		
	// setup
	ftype = FunctionType::get(Type::getVoidTy(ctx), false);
	setup = Function::Create(ftype, Function::ExternalLinkage, "setup", module);
	setup->setCallingConv(CallingConv::C);

	// init 
	ftype = FunctionType::get(Type::getVoidTy(ctx), false);
	init = Function::Create(ftype, Function::ExternalLinkage, "init", module);
	init ->setCallingConv(CallingConv::C);
	#endif
}

#endif // ENABLE_ARDUINO
