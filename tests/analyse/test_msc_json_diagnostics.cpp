#include "../test_macros.hpp"

import spp.analyse.errors.diagnostic_sink;
import spp.lsp.diagnostic;
import spp.lsp.resolution_index;

// The json diagnostics read a raised error as data - a code, a title, and a labelled span per place the error points
// at - rather than as the block it prints. The positions are what an editor is handed, so they are zero-based, and
// counted in UTF-16 code units; these tests pin both, because nothing about the rendered block would notice them
// drifting by one.

TEST(MscJsonDiagnostics, test_semantic_error_carries_code_title_and_spans) {
    try {
        // "fun main" is prepended by the harness, and the raw string opens with a newline, so the "sup" is the third
        // line of the module: line 2, counting from zero.
        build_temp_project(R"(
sup Bool ext Copy { }
)");
        ADD_FAILURE() << "Expected the external marker error, but nothing was thrown.";
    }
    catch (spp::analyse::errors::SemanticError const &e) {
        const auto diagnostic = spp::lsp::DiagnosticFrom(e);
        EXPECT_EQ(diagnostic.Code, "E111");
        EXPECT_EQ(diagnostic.Severity, "error");
        EXPECT_EQ(diagnostic.Title, "Superimposition External Marker Extension Error");
        EXPECT_FALSE(diagnostic.Note.empty());
        EXPECT_FALSE(diagnostic.Help.empty());

        // The context block comes first, as it does in the printed error, and the error's own block is the primary
        // label an editor underlines.
        ASSERT_EQ(diagnostic.Labels.Len(), 2u);
        auto const &context = diagnostic.Labels[0];
        auto const &primary = diagnostic.Labels[1];

        EXPECT_FALSE(context.Primary);
        EXPECT_FALSE(context.Span.Generated);
        EXPECT_EQ(context.Span.StartLine, 2u);
        EXPECT_EQ(context.Span.StartCol, 4u);   // "Bool"
        EXPECT_EQ(context.Span.EndCol, 8u);

        EXPECT_TRUE(primary.Primary);
        EXPECT_FALSE(primary.Span.Generated);
        EXPECT_EQ(primary.Span.StartLine, 2u);
        EXPECT_EQ(primary.Span.StartCol, 13u);  // "Copy"
        EXPECT_EQ(primary.Span.EndCol, 17u);
        EXPECT_EQ(primary.Span.EndLine, primary.Span.StartLine);
    }
}

TEST(MscJsonDiagnostics, test_json_names_every_part_of_the_diagnostic) {
    try {
        build_temp_project(R"(
sup Bool ext Copy { }
)");
        ADD_FAILURE() << "Expected the external marker error, but nothing was thrown.";
    }
    catch (spp::analyse::errors::SemanticError const &e) {
        const auto json = spp::lsp::ToJson(spp::lsp::DiagnosticFrom(e));
        EXPECT_NE(json.find(R"("code":"E111")"), spp::Str::npos);
        EXPECT_NE(json.find(R"("severity":"error")"), spp::Str::npos);
        EXPECT_NE(json.find(R"("primary":true)"), spp::Str::npos);
        EXPECT_NE(json.find(R"("start":{"line":2,"character":13})"), spp::Str::npos);
        EXPECT_NE(json.find(R"("end":{"line":2,"character":17})"), spp::Str::npos);

        // One line, and no escape sequences from the colours the printed block carries.
        EXPECT_EQ(json.find('\n'), spp::Str::npos);
        EXPECT_EQ(json.find('\x1b'), spp::Str::npos);
    }
}

TEST(MscJsonDiagnostics, test_message_colours_are_not_carried_into_the_diagnostic) {
    try {
        // "INLINE_INFO" and friends splice colour escapes into the tag itself, so this error's message is coloured
        // in the middle rather than around the edges - the one shape a diagnostic must not inherit.
        build_temp_project(R"(
fun f() -> Void {
    let x = undefined_thing
}
)");
        ADD_FAILURE() << "Expected the unknown identifier error, but nothing was thrown.";
    }
    catch (spp::analyse::errors::SemanticError const &e) {
        const auto diagnostic = spp::lsp::DiagnosticFrom(e);
        const auto primary = diagnostic.Labels[0];

        EXPECT_EQ(primary.Message.find('\x1b'), spp::Str::npos) << "message: " << primary.Message;
        EXPECT_EQ(diagnostic.Title.find('\x1b'), spp::Str::npos);
        EXPECT_EQ(spp::lsp::ToJson(diagnostic).find('\x1b'), spp::Str::npos);
        EXPECT_TRUE(primary.Message.starts_with("Unknown identifier")) << "message: " << primary.Message;
    }
}

namespace {
    /// Turns recovery on for one test and off again afterwards, whatever the test does - several tests share a process
    /// under a "--gtest_filter" run, and a sink left on would change what the next one sees.
    struct RecoveringCompile {
        RecoveringCompile() { spp::analyse::errors::diagnostic_sink::Enable(true); }

        ~RecoveringCompile() {
            spp::analyse::errors::diagnostic_sink::Enable(false);
            spp::analyse::errors::diagnostic_sink::Clear();
        }
    };
}

TEST(MscJsonDiagnostics, test_recovery_reports_one_error_per_member) {
    const auto recovering = RecoveringCompile();

    // Three functions, each broken in its own right. Without recovery the first would be the only one reported.
    build_temp_project(R"(
fun first() -> Void {
    let a = undefined_one
}

fun second() -> Void {
    let b = undefined_two
}

fun third() -> Void {
    let c = undefined_three
}
)");

    auto lines = spp::Vec<std::size_t>();
    for (auto const &error : spp::analyse::errors::diagnostic_sink::Collected()) {
        const auto diagnostic = spp::lsp::DiagnosticFrom(error);
        EXPECT_EQ(diagnostic.Code, "E26");
        for (auto const &label : diagnostic.Labels) {
            if (label.Primary) { lines.EmplaceBack(label.Span.StartLine); }
        }
    }

    // One per function, wherever in the file they sit; the order they were raised in follows stage 1's lowering
    // rather than the source, so the lines are compared as a set.
    std::ranges::sort(lines);
    ASSERT_EQ(lines.Len(), 3u);
    EXPECT_EQ(lines[0], 3u);
    EXPECT_EQ(lines[1], 7u);
    EXPECT_EQ(lines[2], 11u);
}

TEST(MscJsonDiagnostics, test_recovery_reports_a_member_once_however_many_mistakes_it_holds) {
    const auto recovering = RecoveringCompile();

    // A member is the unit of recovery: once one has failed, the rest of it is not analysed, so the second mistake
    // here is not reported until the first is fixed.
    build_temp_project(R"(
fun only_one() -> Void {
    let a = undefined_one
    let b = undefined_two
}
)");

    EXPECT_EQ(spp::analyse::errors::diagnostic_sink::Collected().Len(), 1u);
}

TEST(MscJsonDiagnostics, test_a_clean_module_recovers_nothing) {
    const auto recovering = RecoveringCompile();
    build_temp_project(R"(
fun fine() -> Void {
    let a = 1
}
)");
    EXPECT_FALSE(spp::analyse::errors::diagnostic_sink::HasErrors());
}

namespace {
    /// Records what every name in the test's own module resolved to, and stops again afterwards. The project scope
    /// rather than everything: the fixture keeps its "std" under "vcs" exactly as a real project does, and indexing a
    /// dependency's every scope is a great deal of work for a test that asks about one file.
    struct IndexedCompile {
        explicit IndexedCompile() { spp::lsp::resolution_index::EnableProject(); }

        ~IndexedCompile() {
            spp::lsp::resolution_index::Disable();
            spp::lsp::resolution_index::Clear();
        }
    };
}

TEST(MscJsonDiagnostics, test_the_index_says_what_a_name_means_and_where_it_was_declared) {
    const auto indexed = IndexedCompile();

    // "fun main" is prepended and the raw string opens with a newline, so "cls Thing" is the third line, counting
    // from zero.
    build_temp_project(R"(
cls Thing { !public v: S32 }

fun read(t: &Thing) -> S32 {
    ret t.v
}
)");

    auto const &entries = spp::lsp::resolution_index::GetAllEntries();

    // The type written in the parameter list resolves to the class declared above it.
    const auto thing = std::ranges::find_if(entries, [](auto const &e) {
        return e.Kind == "type" and e.Name == "Thing" and e.Use.StartLine == 4;
    });
    ASSERT_NE(thing, entries.end()) << "no use of 'Thing' was recorded on the parameter's line";
    EXPECT_FALSE(thing->Definition.Generated);
    EXPECT_EQ(thing->Definition.StartLine, 2u) << "the declaration is the 'cls Thing' line";

    // A name that stands for a value carries the type it has, which is what hover shows.
    const auto value = std::ranges::find_if(entries, [](auto const &e) {
        return e.Kind == "variable" and e.Name == "t";
    });
    ASSERT_NE(value, entries.end()) << "no use of 't' was recorded";
    EXPECT_FALSE(value->Type.empty());
    EXPECT_FALSE(value->Definition.Generated);
}

TEST(MscJsonDiagnostics, test_the_index_follows_member_access_and_namespace_paths) {
    const auto indexed = IndexedCompile();

    build_temp_project(R"(
cls Holder { !public v: S32 }

fun use_them(h: &Holder) -> Void {
    let a = h.v
    std::mem::ops::drop(a)
}
)");

    auto const &entries = spp::lsp::resolution_index::GetAllEntries();
    const auto named = [&entries](spp::Str const &kind, spp::Str const &name) {
        return std::ranges::find_if(entries, [&](auto const &e) { return e.Kind == kind and e.Name == name; });
    };

    // "h.v" names the attribute declared on the class above.
    const auto attribute = named("attribute", "v");
    ASSERT_NE(attribute, entries.end()) << "the attribute reached by '.' was not recorded";
    EXPECT_FALSE(attribute->Definition.Generated);
    EXPECT_EQ(attribute->Definition.StartLine, 2u) << "the declaration is the 'cls Holder' line";

    // The folders a path goes through, and the function at the end of it, are each what the cursor can sit on.
    const auto folder = named("namespace", "ops");
    ASSERT_NE(folder, entries.end()) << "a namespace in the path was not recorded";
    EXPECT_TRUE(folder->Definition.File.ends_with(".spp")) << "file: " << folder->Definition.File;

    const auto function = named("function", "drop");
    ASSERT_NE(function, entries.end()) << "the function at the end of the path was not recorded";
    EXPECT_FALSE(function->Definition.Generated);
    EXPECT_TRUE(function->Definition.File.ends_with("ops.spp")) << "file: " << function->Definition.File;
}

TEST(MscJsonDiagnostics, test_the_index_lists_what_a_type_and_a_namespace_hold) {
    const auto indexed = IndexedCompile();

    build_temp_project(R"(
fun probe() -> Void {
    let v = Vec[S32]()
    std::mem::ops::drop(v)
}
)");

    auto const &entries = spp::lsp::resolution_index::GetAllEntries();
    auto const &lists = spp::lsp::resolution_index::GetAllMembers();

    const auto owner_of = [&entries](spp::Str const &name) {
        const auto found = std::ranges::find_if(entries, [&](auto const &e) { return e.Name == name; });
        return found != entries.end() ? found->Type : spp::Str();
    };
    const auto list_for = [&lists](spp::Str const &owner) {
        return std::ranges::find_if(lists, [&](auto const &l) { return l.Owner == owner; });
    };
    const auto holds = [](auto const &list, spp::Str const &name) {
        return std::ranges::any_of(list->Members, [&](auto const &m) { return m.Name == name; });
    };

    // A value's type is named the same way in the name's own entry and in the list of what that type holds, which is
    // what lets an editor answer "what can follow this dot".
    const auto vec_owner = owner_of("v");
    ASSERT_FALSE(vec_owner.empty());
    const auto vec_members = list_for(vec_owner);
    ASSERT_NE(vec_members, lists.end()) << "no member list for " << vec_owner;
    EXPECT_TRUE(holds(vec_members, "append")) << "a method of the type is missing from its list";

    // And only what could be written here: "length" is the vector's own, declared with package visibility, so a
    // module outside "std" cannot name it and is not offered it.
    EXPECT_FALSE(holds(vec_members, "length")) << "a member this module cannot reach was offered";

    // And the same for what follows a "::" on a namespace.
    const auto ops_members = list_for(owner_of("ops"));
    ASSERT_NE(ops_members, lists.end()) << "no member list for the namespace";
    EXPECT_TRUE(holds(ops_members, "drop"));
}

TEST(MscJsonDiagnostics, test_the_index_lists_the_types_a_type_and_a_namespace_hold) {
    const auto indexed = IndexedCompile();

    build_temp_project(R"(
cls Wrap[T] { }

sup [T] Wrap[T] {
    !public type Element = T
    !private type Hidden = Bool
}

fun probe() -> Void {
    let w = Wrap[Bool]()
    let t = std::threading::thread::current()
    std::mem::ops::drop(w)
    std::mem::ops::drop(t)
}
)");

    auto const &entries = spp::lsp::resolution_index::GetAllEntries();
    auto const &lists = spp::lsp::resolution_index::GetAllMembers();

    const auto owner_of = [&entries](spp::Str const &name) {
        const auto found = std::ranges::find_if(entries, [&](auto const &e) { return e.Name == name; });
        return found != entries.end() ? found->Type : spp::Str();
    };
    const auto list_for = [&lists](spp::Str const &owner) {
        return std::ranges::find_if(lists, [&](auto const &l) { return l.Owner == owner; });
    };
    const auto holds_type = [](auto const &list, spp::Str const &name) {
        return std::ranges::any_of(
            list->Members, [&](auto const &m) { return m.Name == name and m.Kind == "type"; });
    };

    // A nested type is reached through "::" on the type that holds it, so it belongs in that type's list beside its
    // methods - and it is declared in a "sup" block, the only place a "type" statement can sit.
    const auto wrap_members = list_for(owner_of("w"));
    ASSERT_NE(wrap_members, lists.end()) << "no member list for the type";
    EXPECT_TRUE(holds_type(wrap_members, "Element")) << "a nested type was not offered";

    // And only what could be written here: the private one belongs to the type, not to whoever names it.
    EXPECT_FALSE(holds_type(wrap_members, "Hidden")) << "a nested type this module cannot reach was offered";

    // Nor the names that are reachable through the table but are not names: the "Self" every type carries, and the
    // generic parameter this one was instantiated with.
    EXPECT_FALSE(holds_type(wrap_members, "Self")) << "\"Self\" is a resolution mechanism, not a name to offer";
    EXPECT_FALSE(holds_type(wrap_members, "T")) << "a generic parameter was offered as a nested type";

    // The same for a module: the types it declares are named through it.
    const auto thread_members = list_for(owner_of("thread"));
    ASSERT_NE(thread_members, lists.end()) << "no member list for the namespace";
    EXPECT_TRUE(holds_type(thread_members, "JoinHandle")) << "a type the module declares was not offered";
}

// A non-generic class's members all live in its "sup" blocks, and its own block names it before those blocks are
// attached - so the list has to wait for the sup graph rather than answer "nothing" and mark the type seen.
TEST(MscJsonDiagnostics, test_the_index_lists_a_non_generic_types_nested_type) {
    const auto indexed = IndexedCompile();

    build_temp_project(R"(
cls Holder { }

sup Holder {
    !public type Inner = Bool
    !public fun m(&self) -> Void { }
}

fun probe() -> Void {
    let h = Holder()
    h.m()
    std::mem::ops::drop(h)
}
)");

    auto const &entries = spp::lsp::resolution_index::GetAllEntries();
    auto const &lists = spp::lsp::resolution_index::GetAllMembers();

    const auto holder = std::ranges::find_if(entries, [](auto const &e) { return e.Name == "h"; });
    ASSERT_NE(holder, entries.end());
    const auto members = std::ranges::find_if(lists, [&](auto const &l) { return l.Owner == holder->Type; });
    ASSERT_NE(members, lists.end()) << "no member list for " << holder->Type;

    const auto holds = [&members](spp::Str const &name) {
        return std::ranges::any_of(members->Members, [&](auto const &m) { return m.Name == name; });
    };
    EXPECT_TRUE(holds("m")) << "a method the type's sup block declares was not offered";
    EXPECT_TRUE(holds("Inner")) << "a nested type the type's sup block declares was not offered";
}

TEST(MscJsonDiagnostics, test_the_index_follows_a_named_argument_to_what_it_names) {
    const auto indexed = IndexedCompile();

    build_temp_project(R"(
cls Point { !public x: S32 }

fun shifted(base: S32) -> S32 { ret base }

fun probe() -> Void {
    let p = Point(x=1_s32)
    let n = shifted(base=2_s32)
    std::mem::ops::drop(n)
    std::mem::ops::drop(p)
}
)");

    auto const &entries = spp::lsp::resolution_index::GetAllEntries();
    const auto at = [&entries](spp::Str const &kind, std::size_t line) {
        return std::ranges::find_if(entries, [&](auto const &e) {
            return e.Kind == kind and e.Use.StartLine == line;
        });
    };

    // An object initializer's argument names an attribute, and a call's names a parameter; both are written where
    // the cursor can sit, and both have a declaration to be taken to.
    const auto attribute = at("attribute", 7u);
    ASSERT_NE(attribute, entries.end()) << "the object initializer's argument name was not recorded";
    EXPECT_EQ(attribute->Name, "x");
    EXPECT_EQ(attribute->Definition.StartLine, 2u);

    const auto parameter = at("parameter", 8u);
    ASSERT_NE(parameter, entries.end()) << "the call's argument name was not recorded";
    EXPECT_EQ(parameter->Name, "base");
    EXPECT_EQ(parameter->Definition.StartLine, 4u);

    // A positional argument is given a name by the matching, and that name is not something anyone wrote.
    const auto invented = std::ranges::find_if(entries, [](auto const &e) {
        return e.Kind == "parameter" and e.Name == "val";
    });
    EXPECT_EQ(invented, entries.end()) << "a name the matching invented was recorded as if it had been written";
}

TEST(MscJsonDiagnostics, test_the_index_says_what_a_call_takes_and_what_each_place_can_name) {
    const auto indexed = IndexedCompile();

    build_temp_project(R"(
fun shifted(base: S32, by: S32) -> S32 { ret base }

fun probe() -> Void {
    let total = shifted(base=1_s32, by=2_s32)
    std::mem::ops::drop(total)
}
)");

    // The call's own brackets, and the parameters they are filled with.
    auto const &signatures = spp::lsp::resolution_index::GetAllSignatures();
    const auto call = std::ranges::find_if(signatures, [](auto const &s) { return s.Name == "shifted"; });
    ASSERT_NE(call, signatures.end()) << "the call was not recorded";
    ASSERT_EQ(call->Params.Len(), 2u);
    EXPECT_EQ(call->Params[0].Name, "base");
    EXPECT_EQ(call->Params[1].Name, "by");
    EXPECT_FALSE(call->Arguments.Generated);

    // And what can be named inside the function body: its own locals, and the module around it.
    auto const &scopes = spp::lsp::resolution_index::GetAllScopes();
    const auto holds = [](auto const &scope, spp::Str const &name) {
        return std::ranges::any_of(scope.Names, [&](auto const &m) { return m.Name == name; });
    };
    const auto body = std::ranges::find_if(scopes, [&](auto const &s) { return holds(s, "total"); });
    ASSERT_NE(body, scopes.end()) << "the names a function body holds were not recorded";
    EXPECT_GT(body->Where.EndLine, body->Where.StartLine) << "a scope covers more than the line it starts on";

    const auto module = std::ranges::find_if(scopes, [&](auto const &s) { return holds(s, "shifted"); });
    ASSERT_NE(module, scopes.end()) << "the module's own names were not recorded";
}

TEST(MscJsonDiagnostics, test_the_index_stops_at_an_alias_but_walks_through_an_import) {
    const auto indexed = IndexedCompile();

    build_temp_project(R"(
fun gives() -> Opt[S32] {
    ret Some(val=1_s32)
}

fun probe() -> Void {
    let v = Vec[S32]()
    std::mem::ops::drop(v)
}
)");

    auto const &entries = spp::lsp::resolution_index::GetAllEntries();
    const auto type_named = [&entries](spp::Str const &name) {
        return std::ranges::find_if(entries, [&](auto const &e) { return e.Kind == "type" and e.Name == name; });
    };

    // "Opt" is written as "type Opt[T] = Some[T] or None", and that line is the declaration someone reading the name
    // wants - not the variant class it happens to resolve to.
    const auto opt = type_named("Opt");
    ASSERT_NE(opt, entries.end());
    EXPECT_TRUE(opt->Definition.File.ends_with("option.spp")) << "file: " << opt->Definition.File;

    // "Vec" arrives through a "use" in the prelude, which is not a declaration of anything: the class is.
    const auto vec = type_named("Vec");
    ASSERT_NE(vec, entries.end());
    EXPECT_TRUE(vec->Definition.File.ends_with("vector.spp")) << "file: " << vec->Definition.File;
}

TEST(MscJsonDiagnostics, test_the_index_says_what_a_comptime_constant_came_to) {
    const auto indexed = IndexedCompile();

    build_temp_project(R"(
cmp small: S32 = 6_s32
cmp doubled: S32 = small + small

fun probe() -> Void {
    let n = doubled
    std::mem::ops::drop(n)
}
)");

    // The constant is computed while the program is compiled, so the answer exists rather than the expression that
    // would produce it.
    auto const &values = spp::lsp::resolution_index::GetAllComptimeValues();
    const auto named = [&values](spp::Str const &name) {
        return std::ranges::find_if(values, [&](auto const &v) { return v.Name == name; });
    };

    const auto small = named("small");
    ASSERT_NE(small, values.end()) << "the declaration's value was not recorded";
    EXPECT_EQ(small->Value, "6_s32");

    const auto doubled = named("doubled");
    ASSERT_NE(doubled, values.end());
    EXPECT_EQ(doubled->Value, "12_s32") << "the expression was recorded rather than what it came to";

    // And a name that reaches the constant carries the answer too, though it was recorded a stage before there was
    // one to carry.
    auto const &entries = spp::lsp::resolution_index::GetAllEntries();
    const auto use = std::ranges::find_if(entries, [](auto const &e) {
        return e.Name == "doubled" and e.Use.StartLine == 6;
    });
    ASSERT_NE(use, entries.end()) << "the use of the constant was not recorded";
    EXPECT_EQ(use->Value, "12_s32");

    // A function is held as a mock that carries a compile-time value of its own, which means nothing to a reader.
    const auto function = std::ranges::find_if(entries, [](auto const &e) { return e.Kind == "function"; });
    if (function != entries.end()) { EXPECT_TRUE(function->Value.empty()) << "value: " << function->Value; }
}
