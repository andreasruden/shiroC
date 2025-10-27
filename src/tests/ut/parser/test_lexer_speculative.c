#include "parser/lexer.h"
#include "test_runner.h"

#include <string.h>

TEST_FIXTURE(lexer_speculative_fixture_t)
{
    lexer_t* lexer;
};

TEST_SETUP(lexer_speculative_fixture_t)
{
    fix->lexer = nullptr;
}

TEST_TEARDOWN(lexer_speculative_fixture_t)
{
    if (fix->lexer != nullptr)
        lexer_destroy(fix->lexer);
}

TEST(lexer_speculative_fixture_t, test_speculative_commit)
{
    fix->lexer = lexer_create("test.shiro", "var x = 42;", nullptr, nullptr);

    // Enter speculative mode
    lexer_enter_speculative_mode(fix->lexer);

    // Consume some tokens speculatively
    token_t* tok1 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_VAR, tok1->type);

    token_t* tok2 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok2->type);
    ASSERT_EQ("x", tok2->value);

    token_t* tok3 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_ASSIGN, tok3->type);

    // Commit the speculation
    lexer_commit_speculation(fix->lexer);

    // Next token should be the number 42
    token_t* tok4 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_INTEGER, tok4->type);
    ASSERT_EQ("42", tok4->value);
}

TEST(lexer_speculative_fixture_t, test_speculative_rollback)
{
    fix->lexer = lexer_create("test.shiro", "var x = 42;", nullptr, nullptr);

    // Enter speculative mode
    lexer_enter_speculative_mode(fix->lexer);

    // Consume some tokens speculatively
    token_t* tok1 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_VAR, tok1->type);

    token_t* tok2 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok2->type);
    ASSERT_EQ("x", tok2->value);

    token_t* tok3 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_ASSIGN, tok3->type);

    // Rollback the speculation
    lexer_rollback_speculation(fix->lexer);

    // Next token should be back to 'var'
    token_t* tok4 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_VAR, tok4->type);

    token_t* tok5 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok5->type);
    ASSERT_EQ("x", tok5->value);
}

TEST(lexer_speculative_fixture_t, test_speculative_peek_during_speculation)
{
    fix->lexer = lexer_create("test.shiro", "a + b - c", nullptr, nullptr);

    // Enter speculative mode
    lexer_enter_speculative_mode(fix->lexer);

    // Consume 'a'
    token_t* tok1 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok1->type);
    ASSERT_EQ("a", tok1->value);

    // Consume '+'
    token_t* tok2 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_PLUS, tok2->type);

    // Peek should now see 'b'
    token_t* peek1 = lexer_peek_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, peek1->type);
    ASSERT_EQ("b", peek1->value);

    // Peek ahead by 2 should see '-'
    token_t* peek2 = lexer_peek_token_n(fix->lexer, 1);
    ASSERT_EQ(TOKEN_MINUS, peek2->type);

    // Commit the speculation
    lexer_commit_speculation(fix->lexer);

    // Next token should be 'b'
    token_t* tok3 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok3->type);
    ASSERT_EQ("b", tok3->value);
}

TEST(lexer_speculative_fixture_t, test_multiple_speculations_sequential)
{
    fix->lexer = lexer_create("test.shiro", "a b c d e", nullptr, nullptr);

    // First speculation - commit
    lexer_enter_speculative_mode(fix->lexer);
    token_t* tok1 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok1->type);
    ASSERT_EQ("a", tok1->value);
    lexer_commit_speculation(fix->lexer);

    // Second speculation - rollback
    lexer_enter_speculative_mode(fix->lexer);
    token_t* tok2 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok2->type);
    ASSERT_EQ("b", tok2->value);
    lexer_rollback_speculation(fix->lexer);

    // Should be back at 'b'
    token_t* tok3 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok3->type);
    ASSERT_EQ("b", tok3->value);

    // Third speculation - commit
    lexer_enter_speculative_mode(fix->lexer);
    token_t* tok4 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok4->type);
    ASSERT_EQ("c", tok4->value);
    token_t* tok5 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok5->type);
    ASSERT_EQ("d", tok5->value);
    lexer_commit_speculation(fix->lexer);

    // Should be at 'e'
    token_t* tok6 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok6->type);
    ASSERT_EQ("e", tok6->value);
}

TEST(lexer_speculative_fixture_t, test_speculative_empty_consumption)
{
    fix->lexer = lexer_create("test.shiro", "var x;", nullptr, nullptr);

    // Enter speculative mode but don't consume anything
    lexer_enter_speculative_mode(fix->lexer);

    // Commit without consuming
    lexer_commit_speculation(fix->lexer);

    // Should still be at the start
    token_t* tok1 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_VAR, tok1->type);
}

TEST(lexer_speculative_fixture_t, test_speculative_nested_rollback)
{
    fix->lexer = lexer_create("test.shiro", "a b c d e f", nullptr, nullptr);

    // Enter outer speculation
    lexer_enter_speculative_mode(fix->lexer);

    // Consume 'a' in outer speculation
    token_t* tok1 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok1->type);
    ASSERT_EQ("a", tok1->value);

    // Consume 'b' in outer speculation
    token_t* tok2 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok2->type);
    ASSERT_EQ("b", tok2->value);

    // Enter inner speculation
    lexer_enter_speculative_mode(fix->lexer);

    // Consume 'c' in inner speculation
    token_t* tok3 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok3->type);
    ASSERT_EQ("c", tok3->value);

    // Consume 'd' in inner speculation
    token_t* tok4 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok4->type);
    ASSERT_EQ("d", tok4->value);

    // Rollback inner speculation - should go back to position after 'b'
    lexer_rollback_speculation(fix->lexer);

    // Next token should be 'c' again
    token_t* tok5 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok5->type);
    ASSERT_EQ("c", tok5->value);

    // Commit outer speculation
    lexer_commit_speculation(fix->lexer);

    // Next token should be 'd'
    token_t* tok6 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok6->type);
    ASSERT_EQ("d", tok6->value);
}

TEST(lexer_speculative_fixture_t, test_speculative_nested_commit)
{
    fix->lexer = lexer_create("test.shiro", "a b c d e", nullptr, nullptr);

    // Enter outer speculation
    lexer_enter_speculative_mode(fix->lexer);

    // Consume 'a' in outer speculation
    token_t* tok1 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok1->type);
    ASSERT_EQ("a", tok1->value);

    // Enter inner speculation
    lexer_enter_speculative_mode(fix->lexer);

    // Consume 'b' and 'c' in inner speculation
    token_t* tok2 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok2->type);
    ASSERT_EQ("b", tok2->value);

    token_t* tok3 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok3->type);
    ASSERT_EQ("c", tok3->value);

    // Commit inner speculation (but still in outer speculation)
    lexer_commit_speculation(fix->lexer);

    // Continue in outer speculation - next should be 'd'
    token_t* tok4 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok4->type);
    ASSERT_EQ("d", tok4->value);

    // Commit outer speculation
    lexer_commit_speculation(fix->lexer);

    // Next token should be 'e'
    token_t* tok5 = lexer_next_token(fix->lexer);
    ASSERT_EQ(TOKEN_IDENTIFIER, tok5->type);
    ASSERT_EQ("e", tok5->value);
}
