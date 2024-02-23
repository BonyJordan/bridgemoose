#ifndef _JADE_IO_H_
#define _JADE_IO_H_

#include <stdio.h>
#include <string.h>
#include <string>

inline std::string oserr_str(const char* filename = NULL) {
    std::string out = "";
    if (filename != NULL) {
        out = filename;
        out += ": ";
    }
    return out + strerror(errno);
}

template <class T>
struct RESULT {
    T*	         ok;
    std::string  err;

    RESULT() : ok(NULL),err("unset") {}
    RESULT(T* obj) : ok(obj),err() {}
    RESULT(const std::string& s) : ok(NULL),err(s) {}
    RESULT(const RESULT& r) : ok(r.ok),err(r.err) {}
    ~RESULT() {} // we do NOT delete the "ok" object

    struct RESULT& delete_and_error(const std::string& e) {
	if (ok != NULL)
	    delete ok;
	err = e;
	return* this;
    }
};

template <class T>
std::string read_thing(T& thing, FILE* fp)
{
    if (fread(&thing, sizeof thing, 1, fp) != 1) {
        if (feof(fp)) {
            return "Unexpected end of file";
        } else {
            return oserr_str();
        }
    }
    return "";
}

template <class T>
std::string write_thing(T& thing, FILE* fp)
{
    if (fwrite(&thing, sizeof thing, 1, fp) != 1) {
	return oserr_str();
    }
    return "";
}


#endif // _JADE_IO_H_
