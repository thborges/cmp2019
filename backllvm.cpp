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

LLVMContext ctx;
static IRBuilder<> builder(ctx);

Module *module;
BasicBlock *main_stmts;
Function *main_func;

// auxiliary functions of stdc.c
Function *printfloat = NULL;
Function *printdoisints = NULL;

symbol synames[100];

#ifdef ENABLE_ARDUINO
extern Function *analogWrite;
extern Function *analogRead;
extern Function *delay;
extern Function *delayMicroseconds;
extern Function *init;
extern Function *setup;
extern Function *print;
#endif

void create_printfloat() {
	if (printfloat)
		return;
	std::vector<Type*> arg_types;
	arg_types.push_back(Type::getFloatTy(ctx));
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
	main_func = Function::Create(ft, GlobalValue::ExternalLinkage, "main", module);

	main_stmts = BasicBlock::Create(ctx, "entry", main_func);

	builder.SetInsertPoint(main_stmts);

	// declare auxiliary functions from stdc.c
	create_printfloat();
	create_printdoisints();

	#ifdef ENABLE_ARDUINO
	declare_auxiliary_arduino_funcs();	
	builder.CreateCall(init);
	builder.CreateCall(setup);
	#endif
}

Value *default_coersion(Value *v, Type *destty) {
	Type *ty = v->getType();
	if (ty != destty) {
		// float to integer
		if ((ty->isFloatTy() || ty->isDoubleTy()) && destty->isIntegerTy())
			return builder.CreateFPToSI(v, destty);

		// integer to float
		else if ((destty->isFloatTy() || destty->isDoubleTy()) && ty->isIntegerTy())
			return builder.CreateSIToFP(v, destty);

		// generic ext Int to Int
		else if (destty->isIntegerTy() && ty->isIntegerTy())
			return builder.CreateSExtOrTrunc(v, destty);

		// generic trunc
		else
			return builder.CreateTrunc(v, destty);
	}
	return v;
}

Value *gen_llvm_subtree(syntno *no) {
	int i;
	targs *args = no->token_args;
	std::vector<Value *> fargs;
	Value *auxv;
	if (no->type == NO_TOK) {
		switch (no->token) {
			case 'I': return ConstantInt::get(ctx, APInt(16, args->constvalue));
			case 'D': return ConstantFP::get(ctx, APFloat((float)args->constvalue));
			case 'V':
				i = search_symbol(args->varname);
				assert(i != -1); // symbol should exists
				return builder.CreateLoad((Value*)synames[i].llvm, args->varname);
			case 'N': // input IN
				// numero da porta a ser lida
				auxv = gen_llvm_subtree(no->children[0]);
				// converte para Int8 (cf. assinatura da funcao analogRead)
				auxv = default_coersion(auxv, Type::getInt8Ty(ctx));
				fargs.push_back(auxv);
				return builder.CreateCall(analogRead, fargs); 
		}
	}
	else {
		switch (no->type) {
			case NO_ADD:
				return builder.CreateFAdd(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			case NO_SUB:
				return builder.CreateFSub(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			case NO_MULT:
				return builder.CreateFMul(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			case NO_DIV:
				return builder.CreateFDiv(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			case NO_UNA:
				return builder.CreateFMul(
					gen_llvm_subtree(no->children[0]),
					ConstantFP::get(ctx, APFloat(-1.0)));

			case NO_EQ:
				return builder.CreateFCmpOEQ(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			case NO_NE:
				return builder.CreateFCmpONE(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			case NO_LT:
				return builder.CreateFCmpOLT(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			case NO_GT:
				return builder.CreateFCmpOGT(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			case NO_LE:
				return builder.CreateFCmpOLE(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			case NO_GE:
				return builder.CreateFCmpOGE(
					gen_llvm_subtree(no->children[0]),
					gen_llvm_subtree(no->children[1]));

			default:
				printf("Tipo de no nao implementado %d %c.\n", no->type, no->token);
				//exit(1);
		}
	}
	return NULL;
}

Value *generate_llvm_nodes(syntno *no) {

	if (no->type == NO_STMTS || no->type == NO_STMT) {
		BasicBlock *iblock = NULL;
		for(int i = 0; i < no->childcount; i++) {
			Value *ret = generate_llvm_nodes(no->children[i]);
			if (ret && ret->getValueID() == Value::BasicBlockVal) {
				iblock = (BasicBlock*)ret;
				builder.SetInsertPoint(iblock);
			}
		}
		return iblock;

	} else if (no->type == NO_ATTR) {
		Value *var;
		int i = search_symbol(no->children[0]->token_args->varname);
		if (i != -1 && synames[i].llvm)
			var = (Value*)synames[i].llvm;
		else {
			var = builder.CreateAlloca(Type::getFloatTy(ctx), 0, nullptr); // TODO:
			synames[i].llvm = var;
		}
		Value *rvalue = gen_llvm_subtree(no->children[1]);
		rvalue = default_coersion(rvalue, Type::getFloatTy(ctx)); // TODO:
		builder.CreateStore(rvalue, var);

	} else if (no->type == NO_PRNT) {
		// search for symbol to print
		int i = search_symbol(no->token_args->varname);
		assert(i != -1); // symbol should exists
		std::vector<Value *> args;
		Value *loadedvar = builder.CreateLoad((Value*)synames[i].llvm, no->token_args->varname);
		args.push_back(loadedvar);
		builder.CreateCall(printfloat, args); 

	} else if (no->type == NO_WHILE) {

		// a new block for while condition
		BasicBlock *condwhile = BasicBlock::Create(ctx, "while_cond", main_func);
		builder.CreateBr(condwhile);
		builder.SetInsertPoint(condwhile);
		Value *expr = gen_llvm_subtree(no->children[0]);

		// a new block for while body
		BasicBlock *bodywhile = BasicBlock::Create(ctx, "while_body", main_func);
		builder.SetInsertPoint(bodywhile);
		Value *sub_body = generate_llvm_nodes(no->children[1]);

		// a new block for while end (where the program continues after exiting while)
		BasicBlock *endwhile = BasicBlock::Create(ctx, "while_end", main_func);
	
		// after test condition, go to bodywhile or endwhile
		builder.SetInsertPoint(condwhile);
		builder.CreateCondBr(expr, bodywhile, endwhile);

		// return to while condition
		if (sub_body && sub_body->getValueID() == Value::BasicBlockVal)
			builder.SetInsertPoint((BasicBlock*)sub_body);
		else
			builder.SetInsertPoint(bodywhile);
		builder.CreateBr(condwhile);

		return endwhile;

	} else if (no->type == NO_IF) {

		Value *expr = gen_llvm_subtree(no->children[0]);

		// a new block for then
		BasicBlock *then = BasicBlock::Create(ctx, "ifthen", main_func);
		BasicBlock *endif = BasicBlock::Create(ctx, "endif", main_func);

		if (no->childcount == 2) { // only if then, no else
			builder.CreateCondBr(expr, then, endif);
			builder.SetInsertPoint(then);
			Value *retthen = generate_llvm_nodes(no->children[1]);
			builder.CreateBr(endif);
		} else {
			BasicBlock *elseif = BasicBlock::Create(ctx, "else", main_func);
			builder.CreateCondBr(expr, then, elseif);
			
			builder.SetInsertPoint(then);
			Value *retthen = generate_llvm_nodes(no->children[1]);
			builder.CreateBr(endif);
			
			builder.SetInsertPoint(elseif);
			Value *retelse = generate_llvm_nodes(no->children[2]);
			builder.CreateBr(endif);
		}

		return endif;
	}

#ifdef ENABLE_ARDUINO
	else if (no->type == NO_OUT) { // porta saída, chama analogWrite
		Value *port = gen_llvm_subtree(no->children[0]);
		port = default_coersion(port, Type::getInt8Ty(ctx));

		Value *output = gen_llvm_subtree(no->children[1]);
		output = default_coersion(output, Type::getInt16Ty(ctx));

		std::vector<Value *> args;
		args.push_back(port);
		args.push_back(output);
		builder.CreateCall(analogWrite, args);
	}
	else if (no->type == NO_DELAY) { // delay miliseconds
		Value *vdelay = gen_llvm_subtree(no->children[0]);
		vdelay = default_coersion(vdelay, Type::getInt32Ty(ctx));

		std::vector<Value *> args;
		args.push_back(vdelay);
		builder.CreateCall(delay, args);
	}

#endif

	return NULL;
}

void main_generate_llvm_nodes(syntno *no) {
	generate_llvm_nodes(no);
}


