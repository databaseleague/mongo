/**
 *    Copyright (C) 2023-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/db/exec/sbe/expressions/expression.h"
#include "mongo/db/exec/sbe/vm/vm.h"
#include "mongo/db/exec/sbe/vm/vm_printer.h"

#include "mongo/db/exec/sbe/values/block_interface.h"
#include "mongo/db/exec/sbe/values/util.h"
#include "mongo/db/exec/sbe/values/value.h"
#include "mongo/logv2/log.h"
#include "mongo/util/assert_util.h"

#define MONGO_LOGV2_DEFAULT_COMPONENT ::mongo::logv2::LogComponent::kQuery

namespace mongo::sbe::vm {
using ColumnOpType = value::ColumnOpType;

static constexpr auto existsOpType = ColumnOpType{ColumnOpType::kOutputNonNothingOnExpectedInput,
                                                  value::TypeTags::Nothing,
                                                  value::TypeTags::Boolean,
                                                  ColumnOpType::ReturnBoolOnMissing{}};

static const auto existsOp =
    value::makeColumnOp<existsOpType>([](value::TypeTags tag, value::Value val) {
        return std::pair(value::TypeTags::Boolean,
                         value::bitcastFrom<bool>(tag != value::TypeTags::Nothing));
    });

/*
 * Given a ValueBlock as input, returns a ValueBlock of true/false values indicating whether
 * each value in the input was non-Nothing (true) or Nothing (false).
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockExists(ArityType arity) {
    invariant(arity == 1);
    auto [inputOwned, inputTag, inputVal] = getFromStack(0);

    invariant(inputTag == value::TypeTags::valueBlock);
    auto* valueBlockIn = value::bitcastTo<value::ValueBlock*>(inputVal);

    auto out = valueBlockIn->map(existsOp);

    return {
        true, value::TypeTags::valueBlock, value::bitcastFrom<value::ValueBlock*>(out.release())};
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockFillEmpty(
    ArityType arity) {
    MONGO_UNREACHABLE;
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockMin(ArityType arity) {
    MONGO_UNREACHABLE;
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockMax(ArityType arity) {
    MONGO_UNREACHABLE;
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockCount(ArityType arity) {
    MONGO_UNREACHABLE;
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockGtScalar(
    ArityType arity) {
    MONGO_UNREACHABLE;
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockGteScalar(
    ArityType arity) {
    MONGO_UNREACHABLE;
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockEqScalar(
    ArityType arity) {
    MONGO_UNREACHABLE;
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockLtScalar(
    ArityType arity) {
    MONGO_UNREACHABLE;
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockLteScalar(
    ArityType arity) {
    MONGO_UNREACHABLE;
}

/*
 * TODO: Comment.
 */
FastTuple<bool, value::TypeTags, value::Value> ByteCode::builtinValueBlockCombine(ArityType arity) {
    MONGO_UNREACHABLE;
}

static constexpr auto invokeLambdaOpType = ColumnOpType{ColumnOpType::kNoFlags,
                                                        value::TypeTags::Nothing,
                                                        value::TypeTags::Nothing,
                                                        ColumnOpType::OnMissingInput{}};

static const auto invokeLambdaOp =
    value::makeColumnOpWithParams<invokeLambdaOpType, ByteCode::InvokeLambdaFunctor>();

/**
 * Implementation of the valueBlockApplyLambda instruction. This instruction takes a block and an
 * SBE lambda f(), and produces a new block with the result of f() applied to each element of
 * the input.
 */
void ByteCode::valueBlockApplyLambda(const CodeFragment* code) {
    auto [lamOwn, lamTag, lamVal] = moveFromStack(0);
    popAndReleaseStack();
    value::ValueGuard lamGuard(lamOwn, lamTag, lamVal);

    auto [blockOwn, blockTag, blockVal] = moveFromStack(0);
    popAndReleaseStack();
    value::ValueGuard blockGuard(blockOwn, blockTag, blockVal);

    if (lamTag != value::TypeTags::LocalLambda) {
        pushStack(false, value::TypeTags::Nothing, 0);
        return;
    }

    if (blockTag != value::TypeTags::valueBlock) {
        pushStack(false, value::TypeTags::Nothing, 0);
        return;
    }

    const auto lamPos = value::bitcastTo<int64_t>(lamVal);
    auto* block = value::getValueBlock(blockVal);

    auto outBlock = block->map(invokeLambdaOp.bindParams(*this, code, lamPos));

    pushStack(true,
              value::TypeTags::valueBlock,
              value::bitcastFrom<value::ValueBlock*>(outBlock.release()));
}
}  // namespace mongo::sbe::vm
