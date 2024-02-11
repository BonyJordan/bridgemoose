#include "problem.h"
#include "jadeio.h"

static const uint32_t HEADER = 525369437;

std::string PROBLEM::read_from_filestream(FILE* fp)
{
    uint32_t sz;
    std::string err;
    if ((err = read_thing(sz, fp)) != "")
	return err;

    if (sz != HEADER)
	return "Bad PROBLEM header";

    if ((err = read_thing(north, fp)) != "")
	return err;
    if ((err = read_thing(south, fp)) != "")
	return err;
    if ((err = read_thing(trump, fp)) != "")
	return err;
    if ((err = read_thing(target, fp)) != "")
	return err;

    if ((north & ALL_CARDS_BITS) != north ||
        (south & ALL_CARDS_BITS) != south ||
	(north & south) != 0 ||
	handbits_count(north) != 13 ||
	handbits_count(south) != 13 ||
	trump < 0 || trump > 4 ||
	target < 0 || target > 13)
    {
	return "PROBLEM: bit corruption";
    }
    if ((err = read_thing(sz, fp)) != "")
	return err;

    wests.resize(sz);
    easts.resize(sz);
    for (uint32_t i=0 ; i<sz ; i++) {
	hand64_t west;
	if ((err = read_thing(west, fp)) != "")
	    return err;
	if ((west & ALL_CARDS_BITS) != west ||
	    (west & north) != 0 ||
	    (west & south) != 0 ||
	    handbits_count(west) != 13)
	{
	    return "PROBLEM: bit corruption 2";
	}
	wests[i] = west;
	easts[i] = ALL_CARDS_BITS & ~(west | north | south);
    }
    return "";
}

std::string PROBLEM::write_to_filestream(FILE* fp)
{
    std::string err;
    if ((err = write_thing(HEADER, fp)) != "")
	return err;

    if ((err = write_thing(north, fp)) != "")
	return err;
    if ((err = write_thing(south, fp)) != "")
	return err;
    if ((err = write_thing(trump, fp)) != "")
	return err;
    if ((err = write_thing(target, fp)) != "")
	return err;

    uint32_t sz = wests.size();
    if ((err = write_thing(sz, fp)) != "")
	return err;

    for (uint32_t i=0 ; i<sz ; i++) {
	hand64_t west = wests[i];
	if ((err = write_thing(west, fp)) != "")
	    return err;
    }
    return "";
}
