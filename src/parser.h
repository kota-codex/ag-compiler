#ifndef _AK_PARSER_H_
#define _AK_PARSER_H_

#include "ast.h"

using module_text_provider_t = const std::function<std::string(
	std::string name,
	int64_t& version, // in-requestd, out-provided
	std::string& out_path)>&;

void parse(
	ltm::pin<ast::Ast> ast,
	std::string start_module_name,
	module_text_provider_t module_text_provider);

#endif  // _AK_PARSER_H_
