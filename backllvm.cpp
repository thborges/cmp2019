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

#include <llvm/Passes/PassBuilder.h>
#include "llvm/IR/LegacyPassManager.h"

#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

extern "C" {
	#include "header.h"
}

using namespace llvm;

LLVMContext ctx;
static IRBuilder<> builder(ctx);

Module *module;
BasicBlock *main_stmts;
Function *main_func;
Function *current_func;

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
	// main func always returns 0
	Value *retv = ConstantInt::get(ctx, APInt(16, 0));
	builder.CreateRet(retv);

	InitializeAllTargetInfos();
	InitializeAllTargets();
	InitializeAllTargetMCs();
	InitializeAllAsmParsers();
	InitializeAllAsmPrinters();

	auto TargetTriple = "avr-atmel-none";
	std::string Error;
	auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
	auto CPU = "atmega328p";
	auto Features = "+avr5";

	TargetOptions opt;
	auto RM = Optional<Reloc::Model>();
	auto targetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

	module->setDataLayout(targetMachine->createDataLayout());
	module->setTargetTriple(TargetTriple);

	llvm::PassBuilder passBuilder(targetMachine);
	auto loopAnalysisManager = llvm::LoopAnalysisManager{};
	auto functionAnalysisManager = llvm::FunctionAnalysisManager{};
	auto cGSCCAnalysisManager = llvm::CGSCCAnalysisManager{};
	auto moduleAnalysisManager = llvm::ModuleAnalysisManager{};

	passBuilder.registerModuleAnalyses(moduleAnalysisManager);
	passBuilder.registerCGSCCAnalyses(cGSCCAnalysisManager);
	passBuilder.registerFunctionAnalyses(functionAnalysisManager);
	passBuilder.registerLoopAnalyses(loopAnalysisManager);
	passBuilder.crossRegisterProxies(
	    loopAnalysisManager, functionAnalysisManager, cGSCCAnalysisManager, moduleAnalysisManager);

	llvm::ModulePassManager modulePassManager =
    	passBuilder.buildPerModuleDefaultPipeline(llvm::PassBuilder::OptimizationLevel::Oz);
	modulePassManager.run(*module, moduleAnalysisManager);

	//#define ENABLE_STDOUT
	#ifdef ENABLE_STDOUT
	std::string outfilename = filename;
	outfilename += ".o";
	std::error_code ec;
	raw_fd_ostream dest(outfilename, ec);
	if (ec) {
		printf("Error writing to %s.\n", outfilename.c_str());
		exit(1);
	}
	legacy::PassManager pass_codegen;
	targetMachine->addPassesToEmitFile(pass_codegen, dest, nullptr, TargetMachine::CGFT_ObjectFile);
	pass_codegen.run(*module);
	dest.flush();
	#endif


	// print IR to stdout
	module->print(outs(), nullptr);
}

void setup_llvm_global() {

	module = new Module("llvm program", ctx);
	
	FunctionType *ft = FunctionType::get(Type::getInt16Ty(ctx), ArrayRef<Type*>(), false);
	main_func = Function::Create(ft, GlobalValue::ExternalLinkage, "main", module);

	main_stmts = BasicBlock::Create(ctx, "entry", main_func);
	current_func = main_func;

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

Value *default_coersion(Value *v, Type *destty, bool unsig = false) {
	Type *ty = v->getType();
	if (ty != destty) {
		// float to integer
		if ((ty->isFloatTy() || ty->isDoubleTy()) && destty->isIntegerTy()) {
			if (unsig)
				return builder.CreateFPToUI(v, destty);
			else
				return builder.CreateFPToSI(v, destty);
		}

		// integer to float
	else if ((destty->isFloatTy() || destty->isDoubleTy()) && ty->isIntegerTy()) {
			if (unsig)
				return builder.CreateSIToFP(v, destty);
			else
				return builder.CreateUIToFP(v, destty);

		// generic ext Int to Int
		} else if (destty->isIntegerTy() && ty->isIntegerTy())
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
	FunctionType *externft;
	Function *externf;

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
			case 'C':
				// default: call a function without parameters that return float 
				// it should exists in the link stage
				externft = FunctionType::get(Type::getFloatTy(ctx), ArrayRef<Type*>(), false);
				externf = Function::Create(externft, GlobalValue::ExternalLinkage, 
					no->token_args->varname, module);
				return builder.CreateCall(externf);
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
					ConstantFP::get(ctx, APFloat((float)-1.0)));

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
				assert(0 && "Tipo de no nao implementado");
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
		char *varname = no->children[0]->token_args->varname;
		int i = search_symbol(varname);
		if (i != -1 && synames[i].llvm)
			var = (Value*)synames[i].llvm;
		else {
			if (current_func == main_func) { // declare var as global
				GlobalVariable *gv = new GlobalVariable(*module, Type::getFloatTy(ctx), 
					false, GlobalValue::CommonLinkage, NULL, varname);
				gv->setInitializer(ConstantFP::get(ctx, APFloat((float)0.0)));
				var = gv;
			} else {
				var = builder.CreateAlloca(Type::getFloatTy(ctx), 0, nullptr, varname); // TODO:
			}
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
		BasicBlock *condwhile = BasicBlock::Create(ctx, "while_cond", current_func);
		builder.CreateBr(condwhile);
		builder.SetInsertPoint(condwhile);
		Value *expr = gen_llvm_subtree(no->children[0]);

		// a new block for while body
		BasicBlock *bodywhile = BasicBlock::Create(ctx, "while_body", current_func);
		builder.SetInsertPoint(bodywhile);
		Value *sub_body = generate_llvm_nodes(no->children[1]);

		// a new block for while end (where the program continues after exiting while)
		BasicBlock *endwhile = BasicBlock::Create(ctx, "while_end", current_func);
	
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
		BasicBlock *then = BasicBlock::Create(ctx, "ifthen", current_func);
		BasicBlock *endif;

		if (no->childcount == 2) { // only if then, no else
			endif = BasicBlock::Create(ctx, "endif", current_func);
			builder.CreateCondBr(expr, then, endif);
			builder.SetInsertPoint(then);
			Value *retthen = generate_llvm_nodes(no->children[1]);
			builder.CreateBr(endif);
		} else {
			BasicBlock *elseif = BasicBlock::Create(ctx, "else", current_func);
			endif = BasicBlock::Create(ctx, "endif", current_func);
			builder.CreateCondBr(expr, then, elseif);
			
			builder.SetInsertPoint(then);
			Value *retthen = generate_llvm_nodes(no->children[1]);
			if (retthen && retthen->getValueID() == Value::BasicBlockVal)
				builder.SetInsertPoint((BasicBlock*)retthen);
			builder.CreateBr(endif);
			
			builder.SetInsertPoint(elseif);
			Value *retelse = generate_llvm_nodes(no->children[2]);
			if (retelse && retelse->getValueID() == Value::BasicBlockVal)
				builder.SetInsertPoint((BasicBlock*)retelse);
			builder.CreateBr(endif);
		}

		builder.SetInsertPoint(endif);
		return endif;
	}

	else if (no->type == NO_FUNC) {
		BasicBlock *oldblock = builder.GetInsertBlock();

		FunctionType *ft = FunctionType::get(Type::getVoidTy(ctx), ArrayRef<Type*>(), false);
		Function *new_func = Function::Create(ft, GlobalValue::ExternalLinkage, no->token_args->varname, module);

		int i = search_symbol(no->token_args->varname);
		assert(i != -1 && "symbol should exists");
		synames[i].llvm = new_func;

		BasicBlock *newf_stmts = BasicBlock::Create(ctx, "entry", new_func);
		builder.SetInsertPoint(newf_stmts);

		current_func = new_func;
		generate_llvm_nodes(no->children[0]);
		builder.CreateRet(NULL);

		// continue inserting statements on previous set block
		current_func = main_func;
		builder.SetInsertPoint(oldblock);
	}

	else if (no->type == NO_CALL) {

		int i = search_symbol(no->token_args->varname);
		assert(i != -1 && "symbol should exists");
		builder.CreateCall((Function*)synames[i].llvm);
	}

#ifdef ENABLE_ARDUINO
	else if (no->type == NO_OUT) { // porta saída, chama analogWrite
		Value *port = gen_llvm_subtree(no->children[0]);
		port = default_coersion(port, Type::getInt8Ty(ctx), true);

		Value *output = gen_llvm_subtree(no->children[1]);
		//output = default_coersion(output, Type::getInt16Ty(ctx));
		output = default_coersion(output, Type::getInt8Ty(ctx), true);

		std::vector<Value *> args;
		args.push_back(port);
		args.push_back(output);
		builder.CreateCall(analogWrite, args);
	}
	else if (no->type == NO_DELAY) { // delay miliseconds
		Value *vdelay = gen_llvm_subtree(no->children[0]);
		vdelay = default_coersion(vdelay, Type::getInt32Ty(ctx), true);

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


