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

using namespace llvm;

static LLVMContext ctx;
static IRBuilder<> builder(ctx);

Module *module;
BasicBlock *main_stmts;

// auxiliary functions of stdc.c
Function *printfloat = NULL;
Function *printdoisints = NULL;

void create_printfloat() {
	if (printfloat)
		return;
	std::vector<Type*> arg_types;
	arg_types.push_back(Type::getDoubleTy(ctx));
	FunctionType *ftype = FunctionType::get(Type::getVoidTy(ctx), arg_types, false);
	printfloat = Function::Create(ftype, Function::ExternalLinkage, "printfloat", module);
	printfloat->setCallingConv(CallingConv::C);
}

void create_printdoisints() {
	if (printdoisints)
		return;
	std::vector<Type*> arg_types;

	arg_types.push_back(Type::getInt32Ty(ctx));
	arg_types.push_back(Type::getInt32Ty(ctx));

	FunctionType *ftype = FunctionType::get(Type::getVoidTy(ctx), arg_types, false);

	printdoisints = Function::Create(ftype, Function::ExternalLinkage, "print_dois_ints", module);
	printdoisints->setCallingConv(CallingConv::C);
}

void print_llvm_ir() {
	// main func return always 0
	Value *retv = ConstantInt::get(ctx, APInt(16, 0));
	builder.CreateRet(retv);

	// print IR to stdout
	module->print(outs(), nullptr);
}

void setup_llvm_global() {

	module = new Module("llvm program", ctx);
	
	FunctionType *ft = FunctionType::get(Type::getInt16Ty(ctx), ArrayRef<Type*>(), false);
	Function *main_func = Function::Create(ft, GlobalValue::ExternalLinkage, "main", module);

	main_stmts = BasicBlock::Create(ctx, "entry", main_func);

	builder.SetInsertPoint(main_stmts);

	// declare auxiliary functions from stdc.c
	create_printfloat();
	create_printdoisints();
}

Value *gen_llvm_subtree(syntno *no) {
	int i;
	targs *args = no->token_args;
	switch (no->token) {
		case 'I': return ConstantInt::get(ctx, APInt(32, args->constvalue));
		case 'D': return ConstantFP::get(ctx, APFloat(args->constvalue));
		case 'V':
			i = search_symbol(args->varname);
			assert(i != -1); // symbol should exists
			return builder.CreateLoad((Value*)synames[i].llvm, args->varname);
		
		case '+':
			return builder.CreateFAdd(
				gen_llvm_subtree(no->children[0]),
				gen_llvm_subtree(no->children[1]));

		case '-':
			return builder.CreateFSub(
				gen_llvm_subtree(no->children[0]),
				gen_llvm_subtree(no->children[1]));

		case '*':
			return builder.CreateFMul(
				gen_llvm_subtree(no->children[0]),
				gen_llvm_subtree(no->children[1]));

		case '/':
			return builder.CreateFDiv(
				gen_llvm_subtree(no->children[0]),
				gen_llvm_subtree(no->children[1]));

		case 'U':
			return builder.CreateFMul(
				gen_llvm_subtree(no->children[0]),
				ConstantFP::get(ctx, APFloat(-1.0)));

		default:
			printf("Tipo de no nao implementado %d %c.\n", no->type, no->token);
			//exit(1);
	}
}

void generate_llvm_node(syntno **root, syntno *no) {
	syntno *nr = *root;

	if (no->type == NO_ATTR) {
		Value *var;
		int i = search_symbol(no->children[0]->token_args->varname);
		if (i == -1)
			var = (Value*)synames[i].llvm;
		else {
			var = builder.CreateAlloca(Type::getDoubleTy(ctx), 0, nullptr);
			synames[i].llvm = var;
		}
		builder.CreateStore(gen_llvm_subtree(no->children[1]), var);

	} else if (no->type == NO_PRNT) {
		// search for symbol to print
		int i = search_symbol(no->token_args->varname);
		assert(i != -1); // symbol should exists
		std::vector<Value *> args;
		Value *loadedvar = builder.CreateLoad((Value*)synames[i].llvm, no->token_args->varname);
		args.push_back(loadedvar);
		builder.CreateCall(printfloat, args); 
	}
}

