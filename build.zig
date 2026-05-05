const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Build SDL3 via cmake as a static library
    const cmake_configure = b.addSystemCommand(&.{
        "cmake",
        "-S", "vendor/SDL3",
        "-B", "vendor/SDL3/build",
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_C_COMPILER=zig",
        "-DCMAKE_C_COMPILER_ARG1=cc",
        "-DCMAKE_CXX_COMPILER=zig",
        "-DCMAKE_CXX_COMPILER_ARG1=c++",
        "-DSDL_SHARED=OFF",
        "-DSDL_STATIC=ON",
        "-DSDL_TESTS=OFF",
        "-DSDL_EXAMPLES=OFF",
    });

    const cmake_build = b.addSystemCommand(&.{
        "cmake",
        "--build", "vendor/SDL3/build",
        "--config", "Release",
    });
    cmake_build.step.dependOn(&cmake_configure.step);

    // Create module for C compilation
    const mod = b.createModule(.{
        .root_source_file = null,
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    mod.addIncludePath(b.path("vendor/SDL3/include"));
    mod.addIncludePath(b.path("src"));

    // C source files
    const c_flags = &.{
        "-std=c99",
        "-DWOLF3D_SDL3",
        "-gcodeview",
        "-O0",
        "-Wno-pointer-sign",
        "-Wno-implicit-function-declaration",
        "-Wno-incompatible-function-pointer-types",
        "-Wno-deprecated-non-prototype",
        "-Wno-nonportable-include-path",
        "-Wno-int-conversion",
        "-Wno-enum-conversion",
    };

    const sources = [_][]const u8{
        "src/sdl3_main.c",
        "src/sdl3_vid.c",
        "src/sdl3_in.c",
        "src/sdl3_sd.c",
        "src/mcp.c",
        "src/id_ca.c",
        "src/id_in.c",
        "src/id_mm.c",
        "src/id_pm.c",
        "src/id_sd.c",
        "src/id_us_1.c",
        "src/id_vh.c",
        "src/id_vl.c",
        "src/wl_act1.c",
        "src/wl_act2.c",
        "src/wl_agent.c",
        "src/wl_debug.c",
        "src/wl_draw.c",
        "src/wl_game.c",
        "src/wl_inter.c",
        "src/wl_main.c",
        "src/wl_menu.c",
        "src/wl_play.c",
        "src/wl_scale.c",
        "src/wl_state.c",
        "src/wl_text.c",
    };

    inline for (sources) |source| {
        mod.addCSourceFile(.{ .file = b.path(source), .flags = c_flags });
    }

    // Link SDL3 static library
    mod.addLibraryPath(b.path("vendor/SDL3/build"));
    mod.linkSystemLibrary("SDL3", .{});

    // Windows system libraries required by SDL3
    if (target.result.os.tag == .windows) {
        mod.linkSystemLibrary("user32", .{});
        mod.linkSystemLibrary("gdi32", .{});
        mod.linkSystemLibrary("winmm", .{});
        mod.linkSystemLibrary("imm32", .{});
        mod.linkSystemLibrary("ole32", .{});
        mod.linkSystemLibrary("oleaut32", .{});
        mod.linkSystemLibrary("shell32", .{});
        mod.linkSystemLibrary("version", .{});
        mod.linkSystemLibrary("setupapi", .{});
        mod.linkSystemLibrary("dbghelp", .{});
    }

    // Wolf3D executable
    const exe = b.addExecutable(.{
        .name = "wolf3d",
        .root_module = mod,
    });

    exe.step.dependOn(&cmake_build.step);

    b.installArtifact(exe);

    // Add 'zig build run' step
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the game");
    run_step.dependOn(&run_cmd.step);
}
