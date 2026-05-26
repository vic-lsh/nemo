#pragma once

struct ucm_opts;
enum arg_parse_result;

enum arg_parse_result parse_args_impl(struct ucm_opts *opts, int argc,
                                      char *argv[]);
