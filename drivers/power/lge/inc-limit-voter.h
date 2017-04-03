#ifndef _INC_LIMIT_VOTER_H_
#define _INC_LIMIT_VOTER_H_

#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/power_supply.h>

#define LIMIT_VALUE_GRINDER 	10
#define LIMIT_TOTALLY_RELEASED	((INT_MAX/LIMIT_VALUE_GRINDER)*LIMIT_VALUE_GRINDER)
#define LIMIT_TOTALLY_BLOCKED 	0

#define LIMIT_NAME_LENGTH 20

enum limit_type {
	LIMIT_VOTER_INVALID = -1,

	LIMIT_VOTER_IUSB = 0, LIMIT_VOTER_IBAT, LIMIT_VOTER_IDC,

	/* add 'limit_voter_type's here */

	LIMIT_VOTER_MAX,
};

struct limit_voter {
	struct list_head node;
	int id;

	char name[LIMIT_NAME_LENGTH];
	enum limit_type type;
	int limit; // in mA

	int (*activated)(struct limit_voter* entry); // called-back when its charger source is attached.
	int (*effected)(struct limit_voter* entry); // called-back when its voted value is effected.
	int (*deactivated)(struct limit_voter* entry); // called-back when its charger source is detached.
};

static inline union power_supply_propval vote_make(
		enum limit_type limit_type, int limit_current) {
	union power_supply_propval vote = {
			.intval = (limit_current / LIMIT_VALUE_GRINDER) * LIMIT_VALUE_GRINDER + limit_type
	};
	return vote;
}

static inline enum limit_type vote_type(
		const union power_supply_propval* vote) {
	enum limit_type type = LIMIT_VOTER_INVALID;

	switch (vote->intval % LIMIT_VALUE_GRINDER) {
	case LIMIT_VOTER_IUSB:
		type = LIMIT_VOTER_IUSB;
		break;
	case LIMIT_VOTER_IBAT:
		type = LIMIT_VOTER_IBAT;
		break;
	case LIMIT_VOTER_IDC:
		type = LIMIT_VOTER_IDC;
		break;
	default:
		break;
	}

	return type;
}

static inline int vote_current(const union power_supply_propval* vote) {
	return (vote->intval / LIMIT_VALUE_GRINDER) * LIMIT_VALUE_GRINDER;
}

struct limit_voter* limit_voter_getbyname(char* name);
void limit_voter_activate(void);
void limit_voter_deactivate(void);
void limit_voter_set(struct limit_voter* voter, int limit);
void limit_voter_release(struct limit_voter* voter);
void limit_voter_unregister(struct limit_voter* entry);
int limit_voter_register(struct limit_voter* entry, const char* name,
		enum limit_type type,
		int (*activate)(struct limit_voter *entry),
		int (*effected)(struct limit_voter *entry),
		int (*deactivate)(struct limit_voter *entry));

#endif
