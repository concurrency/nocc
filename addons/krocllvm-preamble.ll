declare i32 @llvm.ctlz.i32 (i32)
declare void @llvm.memcpy.i32 (i8*, i8*, i32, i32)
declare { i32, i1 } @llvm.sadd.with.overflow.i32 (i32, i32)
declare { i32, i1 } @llvm.uadd.with.overflow.i32 (i32, i32)
declare { i32, i1 } @llvm.smul.with.overflow.i32 (i32, i32)
declare { i32, i1 } @llvm.ssub.with.overflow.i32 (i32, i32)
declare { i32, i1 } @llvm.usub.with.overflow.i32 (i32, i32)


