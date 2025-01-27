// slang-ir-autodiff-unzip.h
#pragma once

#include "slang-ir.h"
#include "slang-ir-insts.h"
#include "slang-compiler.h"

#include "slang-ir-autodiff.h"
#include "slang-ir-autodiff-fwd.h"
#include "slang-ir-autodiff-propagate.h"
#include "slang-ir-autodiff-transcriber-base.h"
#include "slang-ir-autodiff-region.h"
#include "slang-ir-autodiff-primal-hoist.h"
#include "slang-ir-validate.h"
#include "slang-ir-ssa.h"

namespace Slang
{

struct ParameterBlockTransposeInfo;

struct DiffUnzipPass
{
    AutoDiffSharedContext*                  autodiffContext;

    IRCloneEnv                              cloneEnv;

    DifferentiableTypeConformanceContext    diffTypeContext;

    // Maps used to keep track of primal and
    // differential versions of split insts.
    // 
    Dictionary<IRInst*, IRInst*>            primalMap;
    Dictionary<IRInst*, IRInst*>            diffMap;

    // First diff block.
    // TODO: Can the same pass object can be used for multiple functions?
    // might run into an issue here?
    IRBlock*                                firstDiffBlock;

    RefPtr<IndexedRegionMap>                      indexRegionMap;

    Dictionary<IndexedRegion*, RefPtr<IndexTrackingInfo>> indexInfoMap;

    DiffUnzipPass(
        AutoDiffSharedContext* autodiffContext)
        : autodiffContext(autodiffContext)
        , diffTypeContext(autodiffContext)
    { }

    IRInst* lookupPrimalInst(IRInst* inst)
    {
        return primalMap[inst];
    }

    IRInst* lookupDiffInst(IRInst* inst)
    {
        return diffMap[inst];
    }

    RefPtr<HoistedPrimalsInfo> unzipDiffInsts(IRFunc* func)
    {
        diffTypeContext.setFunc(func);
        
        // Build a map of blocks to loop regions.
        // This will be used later to insert tracking indices
        // 
        indexRegionMap = buildIndexedRegionMap(func);

        IRBuilder builderStorage(autodiffContext->moduleInst->getModule());
        
        IRBuilder* builder = &builderStorage;

        IRFunc* unzippedFunc = func;

        // Initialize the primal/diff map for parameters.
        // Generate distinct references for parameters that should be split.
        // We don't actually modify the parameter list here, instead we emit
        // PrimalParamRef(param) and DiffParamRef(param) and use those to represent
        // a use from the primal or diff part of the program.
        builder->setInsertBefore(unzippedFunc->getFirstBlock()->getTerminator());

        for (auto primalParam = unzippedFunc->getFirstParam(); primalParam; primalParam = primalParam->getNextParam())
        {
            auto type = primalParam->getFullType();
            if (auto ptrType = as<IRPtrTypeBase>(type))
            {
                type = ptrType->getValueType();
            }
            if (auto pairType = as<IRDifferentialPairType>(type))
            {
                IRInst* diffType = diffTypeContext.getDifferentialTypeFromDiffPairType(builder, pairType);
                if (as<IRPtrTypeBase>(primalParam->getFullType()))
                    diffType = builder->getPtrType(primalParam->getFullType()->getOp(), (IRType*)diffType);
                auto primalRef = builder->emitPrimalParamRef(primalParam);
                auto diffRef = builder->emitDiffParamRef((IRType*)diffType, primalParam);
                builder->markInstAsDifferential(diffRef, pairType->getValueType());
                primalMap[primalParam] = primalRef;
                diffMap[primalParam] = diffRef;
            }
        }

        // Functions need to have at least two blocks at this point (one for parameters,
        // and atleast one for code)
        //
        SLANG_ASSERT(unzippedFunc->getFirstBlock() != nullptr);
        SLANG_ASSERT(unzippedFunc->getFirstBlock()->getNextBlock() != nullptr);
        
        IRBlock* firstBlock = as<IRUnconditionalBranch>(unzippedFunc->getFirstBlock()->getTerminator())->getTargetBlock();

        List<IRBlock*> mixedBlocks;
        for (IRBlock* block = firstBlock; block; block = block->getNextBlock())
        {
            // Only need to unzip blocks with both differential and primal instructions.
            if (block->findDecoration<IRMixedDifferentialInstDecoration>())
            {
                mixedBlocks.add(block);
            }
        }

        IRBlock* firstPrimalBlock = nullptr;
        
        // Emit an empty primal block for every mixed block.
        for (auto block : mixedBlocks)
        {
            IRBlock* primalBlock = builder->emitBlock();
            primalMap[block] = primalBlock;

            if (block == firstBlock)
                firstPrimalBlock = primalBlock;
        }

        // Emit an empty differential block for every mixed block.
        for (auto block : mixedBlocks)
        {
            IRBlock* diffBlock = builder->emitBlock(); 
            diffMap[block] = diffBlock;

            // Mark the differential block as a differential inst 
            // (and add a reference to the primal block)
            builder->markInstAsDifferential(diffBlock, nullptr, primalMap[block]);

            // Record the first differential (code) block,
            // since we want all 'return' insts in primal blocks
            // to be replaced with a brahcn into this block.
            // 
            if (block == firstBlock)
                this->firstDiffBlock = diffBlock;
        }

        // Split each block into two. 
        for (auto block : mixedBlocks)
        {
            splitBlock(block, as<IRBlock>(primalMap[block]), as<IRBlock>(diffMap[block]));
        }

        // Emit counter variables and other supporting
        // instructions for all regions.
        // 
        // TODO: Need to have maxIndex in _both_ IndexTrackingInfo & IndexedRegionInfo.
        // That way, we can do the various passes _before_ lowerIndexedRegions()
        // TODO: Remove the call to lowerIndexedRegions() once checkpointing works properly.
        // 
        RefPtr<HoistedPrimalsInfo> primalsInfo = new HoistedPrimalsInfo();
        lowerIndexedRegions(primalsInfo);

        // Copy regions from fwd-block to their split blocks
        // to make it easier to do lookups.
        //
        {
            List<IRBlock*> workList;
            for (auto blockRegionPair : indexRegionMap->map)
            {
                IRBlock* block = blockRegionPair.Key;
                workList.add(block);
            }

            for (auto block : workList)
            {
                if (primalMap.ContainsKey(block))
                    indexRegionMap->map[as<IRBlock>(primalMap[block])] = (IndexedRegion*)indexRegionMap->map[block];
                
                if (diffMap.ContainsKey(block))
                    indexRegionMap->map[as<IRBlock>(diffMap[block])] = (IndexedRegion*)indexRegionMap->map[block];
            }
        }
        
        // Swap the first block's occurences out for the first primal block.
        firstBlock->replaceUsesWith(firstPrimalBlock);

        RefPtr<BlockSplitInfo> splitInfo = new BlockSplitInfo();
        for (auto block : mixedBlocks)
            if (primalMap.ContainsKey(block))
                splitInfo->diffBlockMap[as<IRBlock>(primalMap[block])] = as<IRBlock>(diffMap[block]);
        
        Dictionary<IRBlock*, List<IndexTrackingInfo*>> indexedBlocksInfo;
        for (auto block : mixedBlocks)
        {
            indexedBlocksInfo[as<IRBlock>(diffMap[block])] = getIndexInfoList(as<IRBlock>(diffMap[block]));
            indexedBlocksInfo[as<IRBlock>(primalMap[block])] = getIndexInfoList(as<IRBlock>(primalMap[block]));
        }

        for (auto block : mixedBlocks)
            block->removeAndDeallocate();

        // Run the three checkpointing passes to hoist/clone primal insts
        // to the right spots.
        // 
        {
            RefPtr<AutodiffCheckpointPolicyBase> chkPolicy = new DefaultCheckpointPolicy(unzippedFunc->getModule());
            chkPolicy->preparePolicy(func);

            auto chkPrimalsInfo = chkPolicy->processFunc(func, splitInfo);
            primalsInfo->merge(chkPrimalsInfo);
            
            primalsInfo = ensurePrimalAvailability(primalsInfo, func, indexedBlocksInfo);
        }

        return primalsInfo;
    }

    void tryInferMaxIndex(IndexedRegion* region, IndexTrackingInfo* info)
    {
        if (info->status != IndexTrackingInfo::CountStatus::Unresolved)
            return;

        auto loop = as<IRLoop>(region->getInitializerBlock()->getTerminator());
        
        if (auto maxItersDecoration = loop->findDecoration<IRLoopMaxItersDecoration>())
        {
            info->maxIters = (Count) maxItersDecoration->getMaxIters();
            info->status = IndexTrackingInfo::CountStatus::Static;
        }
        
        if (info->status == IndexTrackingInfo::CountStatus::Unresolved)
        {
            SLANG_UNEXPECTED("Could not resolve max iters \
                for loop appearing in reverse-mode");
        }
    }

    IRInst* addPhiInputParam(IRBuilder* builder, IRBlock* block, IRType* type)
    {
        builder->setInsertInto(block);
        return builder->emitParam(type);
    }

    IRInst* addPhiInputParam(IRBuilder* builder, IRBlock* block, IRType* type, UIndex index)
    {
        List<IRParam*> params;
        for (auto param : block->getParams())
            params.add(param);

        SLANG_RELEASE_ASSERT(index == (UCount)params.getCount());

        return addPhiInputParam(builder, block, type);
    }

    void lowerIndexedRegions(HoistedPrimalsInfo* primalsInfo)
    {
        IRBuilder builder(autodiffContext->moduleInst->getModule());

        for (auto region : indexRegionMap->regions)
        {
            RefPtr<IndexTrackingInfo> info = new IndexTrackingInfo();
            indexInfoMap[region] = info;
            
            // Grab first primal block.
            IRBlock* primalInitBlock = as<IRBlock>(primalMap[region->getInitializerBlock()]);
            builder.setInsertBefore(primalInitBlock->getTerminator());

            // Make variable in the top-most block (so it's visible to diff blocks)
            info->primalCountLastVar = builder.emitVar(builder.getIntType());
            builder.addNameHintDecoration(info->primalCountLastVar, UnownedStringSlice("_pc_last_var"));
            primalsInfo->storeSet.Add(info->primalCountLastVar);
            
            {   
                auto primalCondBlock = as<IRUnconditionalBranch>(
                    primalInitBlock->getTerminator())->getTargetBlock();
                builder.setInsertBefore(primalCondBlock->getTerminator());

                auto phiCounterArgLoopEntryIndex = addPhiOutputArg(
                    &builder,
                    primalInitBlock, 
                    builder.getIntValue(builder.getIntType(), 0));

                info->primalCountParam = addPhiInputParam(
                    &builder,
                    primalCondBlock,
                    builder.getIntType(),
                    phiCounterArgLoopEntryIndex);
                builder.addNameHintDecoration(info->primalCountParam, UnownedStringSlice("_pc"));
                builder.addLoopCounterDecoration(info->primalCountParam);
                builder.markInstAsPrimal(info->primalCountParam);
                
                IRBlock* primalUpdateBlock = as<IRBlock>(primalMap[region->getUpdateBlock()]);
                builder.setInsertBefore(primalUpdateBlock->getTerminator());

                auto incCounterVal = builder.emitAdd(
                    builder.getIntType(), 
                    info->primalCountParam,
                    builder.getIntValue(builder.getIntType(), 1));
                builder.markInstAsPrimal(incCounterVal);

                auto phiCounterArgLoopCycleIndex = addPhiOutputArg(&builder, primalUpdateBlock, incCounterVal);

                SLANG_RELEASE_ASSERT(phiCounterArgLoopEntryIndex == phiCounterArgLoopCycleIndex);

                IRBlock* primalBreakBlock = as<IRBlock>(primalMap[region->getBreakBlock()]);
                builder.setInsertBefore(primalBreakBlock->getTerminator());
                
                builder.emitStore(info->primalCountLastVar, info->primalCountParam);
            }

            {
                IRBlock* diffInitBlock = as<IRBlock>(diffMap[region->getInitializerBlock()]);
                
                auto diffCondBlock = as<IRUnconditionalBranch>(
                    diffInitBlock->getTerminator())->getTargetBlock();
                builder.setInsertBefore(diffCondBlock->getTerminator());

                auto phiCounterArgLoopEntryIndex = addPhiOutputArg(
                    &builder,
                    diffInitBlock, 
                    builder.getIntValue(builder.getIntType(), 0));

                info->diffCountParam = addPhiInputParam(
                    &builder,
                    diffCondBlock,
                    builder.getIntType(),
                    phiCounterArgLoopEntryIndex);
                builder.addNameHintDecoration(info->diffCountParam, UnownedStringSlice("_dc"));
                builder.addLoopCounterDecoration(info->diffCountParam);
                builder.markInstAsPrimal(info->diffCountParam);
                
                IRBlock* diffUpdateBlock = as<IRBlock>(diffMap[region->getUpdateBlock()]);
                builder.setInsertBefore(diffUpdateBlock->getTerminator());

                auto incCounterVal = builder.emitAdd(
                    builder.getIntType(), 
                    info->diffCountParam,
                    builder.getIntValue(builder.getIntType(), 1));
                builder.markInstAsPrimal(incCounterVal);

                auto phiCounterArgLoopCycleIndex = addPhiOutputArg(&builder, diffUpdateBlock, incCounterVal);

                SLANG_RELEASE_ASSERT(phiCounterArgLoopEntryIndex == phiCounterArgLoopCycleIndex);

                auto loopInst = as<IRLoop>(diffInitBlock->getTerminator());

                builder.setInsertBefore(loopInst);

                auto primalCounterLastVal = builder.emitLoad(info->primalCountLastVar);
                builder.markInstAsPrimal(primalCounterLastVal);
                builder.addPrimalValueAccessDecoration(primalCounterLastVal);

                builder.addLoopExitPrimalValueDecoration(loopInst, info->diffCountParam, primalCounterLastVal);

                // We'll be manually creating the inversion entries for the counters
                // TODO: This logic can be moved to the checkpointing alg.
                // 
                primalsInfo->invertSet.Add(info->diffCountParam);
                primalsInfo->instsToInvert.Add(incCounterVal);
                primalsInfo->invertInfoMap[incCounterVal] = InversionInfo(
                    incCounterVal, 
                    List<IRInst*>(incCounterVal), 
                    List<IRInst*>(info->diffCountParam)); 
                
                primalsInfo->invertSet.Add(incCounterVal);
                primalsInfo->instsToInvert.Add(diffUpdateBlock->getTerminator());
                primalsInfo->invertInfoMap[diffUpdateBlock->getTerminator()] = InversionInfo(
                    diffUpdateBlock->getTerminator(), 
                    List<IRInst*>(diffUpdateBlock->getTerminator()), 
                    List<IRInst*>(incCounterVal)); 
            }

            // Try to infer maximum possible number of iterations.
            // (only regions whose intermediates are used outside their region
            // require a maximum count, so we may see some unresolved regions
            // without any issues)
            // 
            tryInferMaxIndex(region, info);
        }
    }

    void tagNewParams(IRBuilder* builder, IRFunc* func)
    {
        for (auto block : func->getBlocks())
        {
            for (auto param = block->getFirstParam(); param; param = param->getNextParam())
                if (!param->findDecoration<IRAutodiffInstDecoration>())
                    builder->markInstAsPrimal(param);
        }
    }

    List<IndexTrackingInfo*> getIndexInfoList(IRBlock* block)
    {
        List<IndexTrackingInfo*> indices;
        for (auto region : indexRegionMap->getAllAncestorRegions(block))
            indices.add((IndexTrackingInfo*) indexInfoMap[region].GetValue());
        
        return indices;
    }

    IRFunc* extractPrimalFunc(
        IRFunc* func,
        IRFunc* originalFunc,
        HoistedPrimalsInfo* primalsInfo,
        ParameterBlockTransposeInfo& paramInfo,
        IRInst*& intermediateType);
    
    static IRInst* _getOriginalFunc(IRInst* call)
    {
        if (auto decor = call->findDecoration<IRAutoDiffOriginalValueDecoration>())
            return decor->getOriginalValue();
        return nullptr;
    }

    InstPair splitCall(IRBuilder* primalBuilder, IRBuilder* diffBuilder, IRCall* mixedCall)
    {
        IRBuilder globalBuilder(autodiffContext->moduleInst->getModule());

        auto fwdCalleeType = mixedCall->getCallee()->getDataType();
        auto baseFn = _getOriginalFunc(mixedCall);
        SLANG_RELEASE_ASSERT(baseFn);

        auto primalFuncType = autodiffContext->transcriberSet.primalTranscriber->differentiateFunctionType(
            primalBuilder, baseFn, as<IRFuncType>(baseFn->getDataType()));

        IRInst* intermediateType = nullptr;

        if (auto specialize = as<IRSpecialize>(baseFn))
        {
            auto func = findSpecializeReturnVal(specialize);
            auto outerGen = findOuterGeneric(func);
            if (func->getOp() == kIROp_LookupWitness)
            {
                // An interface method won't have intermediate type.
                intermediateType = primalBuilder->getVoidType();
            }
            else
            {
                intermediateType = primalBuilder->getBackwardDiffIntermediateContextType(outerGen);
                List<IRInst*> args;
                for (UInt i = 0; i < specialize->getArgCount(); i++)
                    args.add(specialize->getArg(i));
                intermediateType = primalBuilder->emitSpecializeInst(
                    primalBuilder->getTypeKind(),
                    intermediateType,
                    args.getCount(),
                    args.getBuffer());
            }
        }
        else
        {
            if (baseFn->getOp() == kIROp_LookupWitness)
                intermediateType = primalBuilder->getVoidType();
            else
                intermediateType = primalBuilder->getBackwardDiffIntermediateContextType(baseFn);
        }

        IRVar* intermediateVar = nullptr;
        if (!as<IRVoidType>(intermediateType))
        {
            intermediateVar = primalBuilder->emitVar((IRType*)intermediateType);
            primalBuilder->markInstAsPrimal(intermediateVar);
        }
        
        IRInst* primalFn = nullptr;
        if (intermediateVar)
        {
            primalBuilder->addBackwardDerivativePrimalContextDecoration(intermediateVar, intermediateVar);
            primalFn = primalBuilder->emitBackwardDifferentiatePrimalInst(primalFuncType, baseFn);
        }
        else
        {
            // If we decided not to use diff-primal func that stores an reuse context,
            // we can just call the original function instead.
            primalFn = baseFn;
        }
        List<IRInst*> primalArgs;
        for (UIndex ii = 0; ii < mixedCall->getArgCount(); ii++)
        {
            auto arg = mixedCall->getArg(ii);
            if (isRelevantDifferentialPair(arg->getDataType()))
            {
                primalArgs.add(lookupPrimalInst(arg));
            }
            else
            {
                primalArgs.add(arg);
            }
        }
        if (intermediateType->getOp() != kIROp_VoidType)
            primalArgs.add(intermediateVar);

        auto mixedDecoration = mixedCall->findDecoration<IRMixedDifferentialInstDecoration>();
        SLANG_ASSERT(mixedDecoration);

        IRType* primalType = mixedCall->getFullType();
        IRType* diffType = mixedCall->getFullType();
        IRType* resultType = mixedCall->getFullType();
        if (auto fwdPairResultType = as<IRDifferentialPairType>(mixedDecoration->getPairType()))
        {
            primalType = fwdPairResultType->getValueType();
            diffType = (IRType*)diffTypeContext.getDifferentialForType(&globalBuilder, primalType);
            resultType = fwdPairResultType;
        }

        auto primalVal = primalBuilder->emitCallInst(primalType, primalFn, primalArgs);
        if (intermediateVar)
            primalBuilder->addBackwardDerivativePrimalContextDecoration(primalVal, intermediateVar);
        primalBuilder->markInstAsPrimal(primalVal);

        SLANG_RELEASE_ASSERT(mixedCall->getArgCount() <= primalFuncType->getParamCount());

        List<IRInst*> diffArgs;
        for (UIndex ii = 0; ii < mixedCall->getArgCount(); ii++)
        {
            auto arg = mixedCall->getArg(ii);

            // Depending on the type and direction of each argument,
            // we might need to prepare a different value for the transposition logic to produce the
            // correct final argument in the propagate function call.
            if (isRelevantDifferentialPair(arg->getDataType()))
            {
                auto primalArg = lookupPrimalInst(arg);
                auto diffArg = lookupDiffInst(arg);

                if (auto primalVar = as<IRVar>(primalArg))
                {
                    primalArg = diffBuilder->emitVar(as<IRPtrTypeBase>(primalVar->getDataType())->getValueType());
                    if (auto storeUse = findUniqueStoredVal(primalVar))   
                        diffBuilder->emitStore(primalArg, as<IRStore>(storeUse->getUser())->getVal());
                }

                // If arg is a mixed differential (pair), it should have already been split.
                SLANG_ASSERT(primalArg);
                SLANG_ASSERT(diffArg);
                auto primalParamType = primalFuncType->getParamType(ii);
                
                if (auto outType = as<IROutType>(primalParamType))
                {
                    // For `out` parameters that expects an input derivative to propagate through,
                    // we insert a `LoadReverseGradient` inst here to signify the logic in `transposeStore`
                    // that this argument should actually be the currently accumulated derivative on
                    // this variable. The end purpose is that we will generate a load(diffArg) in the
                    // final transposed code and use that as the argument for the call, but we can't just
                    // emit a normal load inst here because the transposition logic will turn loads into stores.
                    auto outDiffType = cast<IRPtrTypeBase>(diffArg->getDataType())->getValueType();
                    auto gradArg = diffBuilder->emitLoadReverseGradient(outDiffType, diffArg);
                    diffBuilder->markInstAsDifferential(gradArg, primalArg->getDataType());
                    diffArgs.add(gradArg);
                }
                else if (auto inoutType = as<IRInOutType>(primalParamType))
                {
                    // Since arg is split into separate vars, we need a new temp var that represents
                    // the remerged diff pair.
                    auto diffPairType = as<IRDifferentialPairType>(as<IRPtrTypeBase>(arg->getDataType())->getValueType());
                    auto primalValueType = diffPairType->getValueType();
                    auto diffPairRef = diffBuilder->emitReverseGradientDiffPairRef(arg->getDataType(), primalArg, diffArg);
                    diffBuilder->markInstAsDifferential(diffPairRef, primalValueType);
                    diffArgs.add(diffPairRef);
                }
                else
                {
                    // For ordinary differentiable input parameters, we make sure to provide
                    // a differential pair. The actual logic that generates an inout variable
                    // will be handled in `transposeCall()`.
                    auto pairArg = diffBuilder->emitMakeDifferentialPair(
                        arg->getDataType(),
                        primalArg,
                        diffArg);

                    diffBuilder->markInstAsDifferential(pairArg, primalArg->getDataType());
                    diffArgs.add(pairArg);
                }
            }
            else
            {
                // For non differentiable arguments, we can simply pass the argument as is
                // if this isn't a `out` parameter, in which case it is removed from propagate call.
                if (!as<IROutType>(arg->getDataType()))
                    diffArgs.add(arg);
            }
        }
        
        auto newFwdCallee = diffBuilder->emitForwardDifferentiateInst(fwdCalleeType, baseFn);

        diffBuilder->markInstAsDifferential(newFwdCallee);

        auto callInst = diffBuilder->emitCallInst(
            resultType,
            newFwdCallee,
            diffArgs);
        diffBuilder->markInstAsDifferential(callInst, primalType);

        if (intermediateVar)
        {
            disableIRValidationAtInsert();
            diffBuilder->addBackwardDerivativePrimalContextDecoration(callInst, intermediateVar);
            enableIRValidationAtInsert();
        }

        IRInst* diffVal = nullptr;
        if (as<IRDifferentialPairType>(callInst->getDataType()))
        {
            diffVal = diffBuilder->emitDifferentialPairGetDifferential(diffType, callInst);
            diffBuilder->markInstAsDifferential(diffVal, primalType);
        }
        return InstPair(primalVal, diffVal);
    }

    InstPair splitMakePair(IRBuilder*, IRBuilder*, IRMakeDifferentialPair* mixedPair)
    {
        return InstPair(mixedPair->getPrimalValue(), mixedPair->getDifferentialValue());
    }

    InstPair splitLoad(IRBuilder* primalBuilder, IRBuilder* diffBuilder, IRLoad* mixedLoad)
    {
        auto primalPtr = lookupPrimalInst(mixedLoad->getPtr());
        auto diffPtr = lookupDiffInst(mixedLoad->getPtr());
        auto primalVal = primalBuilder->emitLoad(primalPtr);
        auto diffVal = diffBuilder->emitLoad(diffPtr);
        diffBuilder->markInstAsDifferential(diffVal, primalVal->getFullType());
        return InstPair(primalVal, diffVal);
    }

    InstPair splitStore(IRBuilder* primalBuilder, IRBuilder* diffBuilder, IRStore* mixedStore)
    {
        auto primalAddr = lookupPrimalInst(mixedStore->getPtr());
        auto diffAddr = lookupDiffInst(mixedStore->getPtr());

        auto primalVal = lookupPrimalInst(mixedStore->getVal());
        auto diffVal = lookupDiffInst(mixedStore->getVal());

        auto primalStore = primalBuilder->emitStore(primalAddr, primalVal);
        auto diffStore = diffBuilder->emitStore(diffAddr, diffVal);

        diffBuilder->markInstAsDifferential(diffStore, primalVal->getFullType());
        return InstPair(primalStore, diffStore);
    }

    InstPair splitVar(IRBuilder* primalBuilder, IRBuilder* diffBuilder, IRVar* mixedVar)
    {
        auto pairType = as<IRDifferentialPairType>(as<IRPtrTypeBase>(mixedVar->getDataType())->getValueType());
        auto primalType = pairType->getValueType();
        auto diffType = (IRType*) diffTypeContext.getDifferentialForType(primalBuilder, primalType);
        auto primalVar = primalBuilder->emitVar(primalType);
        auto diffVar = diffBuilder->emitVar(diffType);
        diffBuilder->markInstAsDifferential(diffVar, diffBuilder->getPtrType(primalType));
        return InstPair(primalVar, diffVar);
    }

    InstPair splitReturn(IRBuilder* primalBuilder, IRBuilder* diffBuilder, IRReturn* mixedReturn)
    {
        auto pairType = as<IRDifferentialPairType>(mixedReturn->getVal()->getDataType());
        // Are we returning a differentiable value?
        if (pairType)
        {
            auto primalType = pairType->getValueType();

            // Check that we have an unambiguous 'first' differential block.
            SLANG_ASSERT(firstDiffBlock);
            
            auto primalBranch = primalBuilder->emitBranch(firstDiffBlock);
            primalBuilder->addBackwardDerivativePrimalReturnDecoration(
                primalBranch, lookupPrimalInst(mixedReturn->getVal()));

            auto pairVal = diffBuilder->emitMakeDifferentialPair(
                pairType,
                lookupPrimalInst(mixedReturn->getVal()),
                lookupDiffInst(mixedReturn->getVal()));
            diffBuilder->markInstAsDifferential(pairVal, primalType);

            auto returnInst = diffBuilder->emitReturn(pairVal);
            diffBuilder->markInstAsDifferential(returnInst, primalType);

            return InstPair(primalBranch, returnInst);
        }
        else
        {
            // If return value is not differentiable, just turn it into a trivial branch.
            auto primalBranch = primalBuilder->emitBranch(firstDiffBlock);
            primalBuilder->addBackwardDerivativePrimalReturnDecoration(
                primalBranch, mixedReturn->getVal());

            auto returnInst = diffBuilder->emitReturn();
            diffBuilder->markInstAsDifferential(returnInst, nullptr);
            return InstPair(primalBranch, returnInst);
        }
    }

    // Splitting a loop is one of the trickiest parts of the unzip pass.
    // Thus far, we've been dealing with blocks that are only run once, so we 
    // could arbitrarily move intermediate instructions to other blocks since they are
    // generated and consumed at-most one time.
    // 
    // Intermediate instructions in a loop can take on a different value each iteration
    // and thus need to be stored explicitly to an array.
    // 
    // We also need to ascertain an upper limit on the iteration count. 
    // With very few exceptions, this is a fundamental requirement.
    // 
    InstPair splitLoop(IRBuilder* primalBuilder, IRBuilder* diffBuilder, IRLoop* mixedLoop)
    {
        
        auto breakBlock = mixedLoop->getBreakBlock();
        auto continueBlock = mixedLoop->getContinueBlock();
        auto nextBlock = mixedLoop->getTargetBlock();

        // Split args.
        List<IRInst*> primalArgs;
        List<IRInst*> diffArgs;
        for (UIndex ii = 0; ii < mixedLoop->getArgCount(); ii++)
        {
            if (isDifferentialInst(mixedLoop->getArg(ii)))
                diffArgs.add(mixedLoop->getArg(ii));
            else
                primalArgs.add(mixedLoop->getArg(ii));
        }

        auto primalLoop = primalBuilder->emitLoop(
            as<IRBlock>(primalMap[nextBlock]),
            as<IRBlock>(primalMap[breakBlock]),
            as<IRBlock>(primalMap[continueBlock]),
            primalArgs.getCount(),
            primalArgs.getBuffer());

        auto diffLoop = diffBuilder->emitLoop(
            as<IRBlock>(diffMap[nextBlock]),
            as<IRBlock>(diffMap[breakBlock]),
            as<IRBlock>(diffMap[continueBlock]),
            diffArgs.getCount(),
            diffArgs.getBuffer());

        if (auto maxItersDecoration = mixedLoop->findDecoration<IRLoopMaxItersDecoration>())
        {
            primalBuilder->addLoopMaxItersDecoration(primalLoop, maxItersDecoration->getMaxIters());
            diffBuilder->addLoopMaxItersDecoration(diffLoop, maxItersDecoration->getMaxIters());
        }

        return InstPair(primalLoop, diffLoop);
    }

    InstPair splitControlFlow(IRBuilder* primalBuilder, IRBuilder* diffBuilder, IRInst* branchInst)
    {
        switch (branchInst->getOp())
        {
        case kIROp_unconditionalBranch:
            {
                auto uncondBranchInst = as<IRUnconditionalBranch>(branchInst);
                auto targetBlock = uncondBranchInst->getTargetBlock();
                
                // Split args.
                List<IRInst*> primalArgs;
                List<IRInst*> diffArgs;
                for (UIndex ii = 0; ii < uncondBranchInst->getArgCount(); ii++)
                {
                    if (isDifferentialInst(uncondBranchInst->getArg(ii)))
                        diffArgs.add(uncondBranchInst->getArg(ii));
                    else
                        primalArgs.add(uncondBranchInst->getArg(ii));
                }
                
                return InstPair(
                    primalBuilder->emitBranch(
                        as<IRBlock>(primalMap[targetBlock]),
                        primalArgs.getCount(),
                        primalArgs.getBuffer()),
                    diffBuilder->emitBranch(
                        as<IRBlock>(diffMap[targetBlock]),
                        diffArgs.getCount(),
                        diffArgs.getBuffer()));

            }
        
        case kIROp_conditionalBranch:
            {
                auto trueBlock = as<IRConditionalBranch>(branchInst)->getTrueBlock();
                auto falseBlock = as<IRConditionalBranch>(branchInst)->getFalseBlock();
                auto condInst = as<IRConditionalBranch>(branchInst)->getCondition();

                return InstPair(
                    primalBuilder->emitBranch(
                        condInst,
                        as<IRBlock>(primalMap[trueBlock]),
                        as<IRBlock>(primalMap[falseBlock])),
                    diffBuilder->emitBranch(
                        condInst,
                        as<IRBlock>(diffMap[trueBlock]),
                        as<IRBlock>(diffMap[falseBlock])));
            }
        
        case kIROp_ifElse:
            {
                auto trueBlock = as<IRIfElse>(branchInst)->getTrueBlock();
                auto falseBlock = as<IRIfElse>(branchInst)->getFalseBlock();
                auto afterBlock = as<IRIfElse>(branchInst)->getAfterBlock();
                auto condInst = as<IRIfElse>(branchInst)->getCondition();

                return InstPair(
                    primalBuilder->emitIfElse(
                        condInst,
                        as<IRBlock>(primalMap[trueBlock]),
                        as<IRBlock>(primalMap[falseBlock]),
                        as<IRBlock>(primalMap[afterBlock])),
                    diffBuilder->emitIfElse(
                        condInst,
                        as<IRBlock>(diffMap[trueBlock]),
                        as<IRBlock>(diffMap[falseBlock]),
                        as<IRBlock>(diffMap[afterBlock])));
            }
        
        case kIROp_Switch:
            {
                auto switchInst = as<IRSwitch>(branchInst);
                auto breakBlock = switchInst->getBreakLabel();
                auto defaultBlock = switchInst->getDefaultLabel();
                auto condInst = switchInst->getCondition();

                List<IRInst*> primalCaseArgs;
                List<IRInst*> diffCaseArgs;

                for (UIndex ii = 0; ii < switchInst->getCaseCount(); ii ++)
                {
                    primalCaseArgs.add(switchInst->getCaseValue(ii));
                    diffCaseArgs.add(switchInst->getCaseValue(ii));

                    primalCaseArgs.add(primalMap[switchInst->getCaseLabel(ii)]);
                    diffCaseArgs.add(diffMap[switchInst->getCaseLabel(ii)]);
                }

                return InstPair(
                    primalBuilder->emitSwitch(
                        condInst,
                        as<IRBlock>(primalMap[breakBlock]),
                        as<IRBlock>(primalMap[defaultBlock]),
                        primalCaseArgs.getCount(),
                        primalCaseArgs.getBuffer()),
                    diffBuilder->emitSwitch(
                        condInst,
                        as<IRBlock>(diffMap[breakBlock]),
                        as<IRBlock>(diffMap[defaultBlock]),
                        diffCaseArgs.getCount(),
                        diffCaseArgs.getBuffer()));
            }
        
        case kIROp_loop:
            return splitLoop(primalBuilder, diffBuilder, as<IRLoop>(branchInst));
        
        default:
            SLANG_UNEXPECTED("Unhandled instruction");
        }
    }
    
    InstPair _splitMixedInst(IRBuilder* primalBuilder, IRBuilder* diffBuilder, IRInst* inst)
    {
        switch (inst->getOp())
        {
        case kIROp_Call:
            return splitCall(primalBuilder, diffBuilder, as<IRCall>(inst));

        case kIROp_Var:
            return splitVar(primalBuilder, diffBuilder, as<IRVar>(inst));

        case kIROp_MakeDifferentialPair:
            return splitMakePair(primalBuilder, diffBuilder, as<IRMakeDifferentialPair>(inst));
 
        case kIROp_Load:
            return splitLoad(primalBuilder, diffBuilder, as<IRLoad>(inst));
        
        case kIROp_Store:
            return splitStore(primalBuilder, diffBuilder, as<IRStore>(inst));

        case kIROp_Return:
            return splitReturn(primalBuilder, diffBuilder, as<IRReturn>(inst));

        case kIROp_unconditionalBranch:
        case kIROp_conditionalBranch:
        case kIROp_ifElse:
        case kIROp_Switch:
        case kIROp_loop:
            return splitControlFlow(primalBuilder, diffBuilder, inst);
        
        case kIROp_Unreachable:
            return InstPair(primalBuilder->emitUnreachable(),
                diffBuilder->emitUnreachable());

        default:
            SLANG_ASSERT_FAILURE("Unhandled mixed diff inst");
        }
    }

    void splitMixedInst(IRBuilder* primalBuilder, IRBuilder* diffBuilder, IRInst* inst)
    {
        auto instPair = _splitMixedInst(primalBuilder, diffBuilder, inst);

        primalMap[inst] = instPair.primal;
        diffMap[inst] = instPair.differential;
    }

    void splitBlock(IRBlock* block, IRBlock* primalBlock, IRBlock* diffBlock)
    {
        // Make two builders for primal and differential blocks.
        IRBuilder primalBuilder(autodiffContext->moduleInst->getModule());
        primalBuilder.setInsertInto(primalBlock);

        IRBuilder diffBuilder(autodiffContext->moduleInst->getModule());
        diffBuilder.setInsertInto(diffBlock);

        List<IRInst*> splitInsts;
        for (auto child : block->getModifiableChildren())
        {
            if (auto getDiffInst = as<IRDifferentialPairGetDifferential>(child))
            {
                // Replace GetDiff(A) with A.d
                if (diffMap.ContainsKey(getDiffInst->getBase()))
                {
                    getDiffInst->replaceUsesWith(lookupDiffInst(getDiffInst->getBase()));
                    getDiffInst->removeAndDeallocate();
                    continue;
                }
            }
            else if (auto getPrimalInst = as<IRDifferentialPairGetPrimal>(child))
            {
                // Replace GetPrimal(A) with A.p
                if (primalMap.ContainsKey(getPrimalInst->getBase()))
                {
                    getPrimalInst->replaceUsesWith(lookupPrimalInst(getPrimalInst->getBase()));
                    getPrimalInst->removeAndDeallocate();
                    continue;
                }
            }

            if (isDifferentialInst(child))
            {
                child->insertAtEnd(diffBlock);
            }
            else if (isMixedDifferentialInst(child))
            {
                splitMixedInst(&primalBuilder, &diffBuilder, child);
                splitInsts.add(child);
            }
            else
            {
                child->insertAtEnd(primalBlock);
            }
        }

        // Remove insts that were split.
        for (auto inst : splitInsts)
        {
            if (!isDifferentiableType(diffTypeContext, inst->getDataType()))
            {
                inst->replaceUsesWith(lookupPrimalInst(inst));
            }

            // Consistency check.
            for (auto use = inst->firstUse; use; use = use->nextUse)
            {
                SLANG_RELEASE_ASSERT((use->getUser()->getParent() != primalBlock) && 
                    (use->getUser()->getParent() != diffBlock));
            }

            // Leave terminator in to keep CFG info.
            if (!as<IRTerminatorInst>(inst))
                inst->removeAndDeallocate();
        }

        // Nothing should be left in the original block.
        SLANG_ASSERT(block->getFirstChild() == block->getTerminator());
    }
};

}
