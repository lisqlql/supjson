#ifndef LYZA_JSON_PARSER__
# define LYZA_JSON_PARSER__

# include <istream>
# include <sstream>
# include <stdexcept>
# include <locale>
# include <cmath>

# include "producer.hpp"
# include "value.hpp"

namespace lyza { namespace json {

namespace {
	bool isdigit(char c)
	{
		// TODO Should we instanciate the locate every times ?
		static std::locale loc;
		return std::isdigit(c, loc);
	}
}

namespace lj = lyza::json;

class parser {

	public:
	class parse_error : public std::runtime_error {
		std::string what_;

		public:
		explicit parse_error(lj::producer& p, const string& what_arg)
			: std::runtime_error("")
		{
			std::stringstream ss;

			ss << p.get_line() << ':' << p.get_column() << ": " << what_arg;
			ss << "\n";

			what_ = ss.str();
		}

		virtual const char* what() const throw()
		{
			return what_.c_str();
		}
	};

	private:
	static void error(lj::producer& p, std::string const& msg)
	{
		std::string until_eof;

		while (!p.eof()) {
			until_eof += p.nextc();
		}

		throw parse_error(p, msg + "until_eof=\"" + until_eof + "\"");
	}

	static bool has(lj::producer& p, char c)
	{
		if (p.peekc() == c) {
			return true;
		}
		return false;
	}

	static bool may_have(lj::producer& p, char c)
	{
		if (p.peekc() == c) {
			p.nextc();
			return true;
		}
		return false;
	}

	static std::string char_to_code(char c)
	{
		std::stringstream ss;

		ss << static_cast<int>(c);
		return ss.str();
	}

	static void expects(lj::producer& p, char c)
	{
		char cc = p.nextc();
		if (cc != c) {
			throw parse_error(p,
					std::string("parse error: ")
					+ "'" + std::string(c, 1) + "'"
					+ "(code=" + char_to_code(c) + ")"
					+ "	expected, got: "
					+ "'" + std::string(cc, 1) + "'"
					+ "' (code=" + char_to_code(cc) + ")");
		}
	}

	static void match_string(lj::producer&p, std::string const& s)
	{
		for (auto c : s) {
			expects(p, c);
		}
	}

	//lj::string parse_string(lj::producer& p);
	//lj::array parse_array(lj::producer& p);
	//lj::boolean parse_bool(lj::producer& p);
	//lj::null parse_null(lj::producer& p);
	//lj::object parse_object(lj::producer& p);
	//lj::value parse_value(lj::producer& p);

	public:

	static lj::string parse_string(lj::producer& p)
	{
		//std::cout << "parse_string" << std::endl;
		std::string acc;

		expects(p, '"');
		p.skip_ws(false);
		while (!has(p, '"')) {
			if (may_have(p, '\\')) {
				acc += p.nextc();
			} else {
				acc += p.nextc();
			}
		}
		expects(p, '"');
		p.skip_ws(true);

		return acc;
	}

	static lj::boolean parse_bool(lj::producer& p)
	{
		lj::boolean b = false;

		if (has(p, 't')) {
			match_string(p, "true");
			b = true;
		} else if (has(p, 'f')) {
			match_string(p, "false");
			b = false;
		} else {
			error(p, "expected boolean");
		}
		return b;
	}

	static lj::null parse_null(lj::producer& p)
	{
		//std::cout << "parse_null" << std::endl;
		if (has(p, 'n')) {
			match_string(p, "null");
		} else {
			error(p, "expected null");
		}
		return lj::null();
	}

	static lj::array parse_array(lj::producer& p)
	{
		//std::cout << "parse_array" << std::endl;
		lj::array a;

		expects(p, '[');
		if (has(p, ']')) { // empty
			p.nextc();
			return a;
		}

		bool more = true;
		while (more) {
			a.push_back(parse_value(p));
			if (has(p, ','))  {
				p.nextc();
				more = true;
			} else
				more = false;
		}
		expects(p, ']');

		return a;
	}

	static lj::object parse_object(lj::producer& p)
	{
		//std::cout << "parse_object" << std::endl;
		lj::object o;

		expects(p, '{');
		if (has(p, '}')) { // empty
			p.nextc();
			return o;
		}

		bool more = true;
		while (more) {
			if (has(p, '"')) {
				lj::string key = parse_string(p);
				expects(p, ':');
				o[key] = parse_value(p);
				if (has(p, ','))  {
					p.nextc();
					more = true;
				} else
					more = false;
			} else {
				error(p, "expected key");
			}
		}
		expects(p, '}');
		return o;
	}

	static lj::number parse_number(lj::producer& p)
	{
		//std::cout << "parse_number" << std::endl;
		bool neg = false;

		if (has(p, '-')) {
			p.nextc();
			neg = true;
		}

		std::string num;
		if (has(p, '0')) {
			num += p.nextc();
		} else {
			if ('0' < p.peekc() && p.peekc() <= '9') {
				num += p.nextc();
			} else {
				error(p, "expected number");
			}
			while (isdigit(p.peekc())) {
				num += p.nextc();
			}
		}

		if (has(p, '.')) {
			num += p.nextc();

			if (isdigit(p.peekc())) {
				num += p.nextc();
			} else {
				error(p, "expected number");
			}
		}


		while (isdigit(p.peekc())) {
			num += p.nextc();
		}

		double dnum = std::stod(num);

		if (has(p, 'e') || has(p, 'E')) {
			p.nextc();

			char op = '+';
			if (has(p, '-') || has(p, '+')) {
				op = p.nextc();
			}

			std::string exp;
			while (isdigit(p.peekc())) {
				exp += p.nextc();
			}

			double dexp = std::pow(10, (op == '-' ? -1. : 1.) * std::stod(exp));

			dnum = dnum * dexp;
		}

		return neg ? -dnum : dnum;
	}

	static lj::value parse_value(lj::producer& p)
	{
		//std::cout << "parse_value" << std::endl;
		lj::value val;

		if (has(p, '"')) {
			val = parse_string(p);
		} else if (has(p, '[')) {
			val = parse_array(p);
		} else if (has(p, '{')) {
			val = parse_object(p);
		} else if (has(p, 't') || has(p, 'f')) {
			val = parse_bool(p);
		} else if (has(p, 'n')) {
			val = parse_null(p);
		} else if (has(p, '-') || isdigit(p.peekc())) {
			val = parse_number(p);
		} else {
			error(p, "expected value");
		}
		return val;
	}

	static lj::object parse(lj::producer& p)
	{
		p.skip_ws(true);
		return parse_object(p);
	}

	static lj::object parse(lj::producer&& p)
	{
		p.skip_ws(true);
		return parse_object(p);
	}

};

}}

#endif
