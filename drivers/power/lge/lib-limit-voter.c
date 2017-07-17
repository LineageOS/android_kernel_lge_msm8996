#define pr_fmt(fmt) "LIMIT-VOTER: %s: " fmt, __func__
#define pr_voter(fmt, ...) pr_info(fmt, ##__VA_ARGS__)

#include <linux/power_supply.h>
#include "inc-limit-voter.h"

/* DO NOT export limit_list */
struct limit_list {
	struct list_head head;
	struct mutex lock;
	enum limit_type type;
};

enum limit_action {
	ACTION_ACTIVE, ACTION_DEACTIVE,
};

// at this time, we have 3 voter classes for IUSB, IBAT, and IDC.
// If you need to add other voter class, add here and update the total list
// for convenient iteration.
static struct limit_list voter_list_iusb = {
	.head = LIST_HEAD_INIT(voter_list_iusb.head),
	.lock = __MUTEX_INITIALIZER(voter_list_iusb.lock),
	.type = LIMIT_VOTER_IUSB,
};
static struct limit_list voter_list_ibat = {
	.head = LIST_HEAD_INIT(voter_list_ibat.head),
	.lock = __MUTEX_INITIALIZER(voter_list_ibat.lock),
	.type = LIMIT_VOTER_IBAT,
};
static struct limit_list voter_list_idc = {
	.head = LIST_HEAD_INIT(voter_list_idc.head),
	.lock = __MUTEX_INITIALIZER(voter_list_idc.lock),
	.type = LIMIT_VOTER_IDC,
};

static struct limit_list* voter_list_total [] = {
	&voter_list_iusb, &voter_list_ibat, &voter_list_idc,
};

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////
static struct limit_voter* limit_voter_get_by_name(struct limit_list* voter_list, char* name);
static struct limit_list* limit_list_get_by_type(enum limit_type type);
static struct limit_list* limit_list_get_by_entry(struct limit_voter* entry);
static int atomic_effecting_limit(struct limit_list* list_of);
static int atomic_voter_id(void);
static void atomic_limit_to_veneer(struct limit_list* list_of);
static void signal_to_voters(struct limit_list* container, enum limit_action action);
static void signal_to_veneer(enum limit_type type, int limit);
///////////////////////////////////////////////////////////////////////////////

static struct limit_voter* limit_voter_get_by_name(
		struct limit_list* voter_list, char* name) {
	struct limit_voter* iter;

	list_for_each_entry(iter, &voter_list->head, node) {
		if (!strcmp(iter->name, name)) {
			return iter;
		}
	}

	return NULL;
}

static struct limit_list* limit_list_get_by_type(
		enum limit_type type) {
	switch (type) {
	case LIMIT_VOTER_IUSB:
		return &voter_list_iusb;
	case LIMIT_VOTER_IBAT:
		return &voter_list_ibat;
	case LIMIT_VOTER_IDC:
		return &voter_list_idc;

	default:
		pr_voter("Invalid limit voter type\n");
		return NULL;
	}
}

static struct limit_list* limit_list_get_by_entry(
		struct limit_voter* entry) {
	return limit_list_get_by_type(entry->type);
}

static int atomic_effecting_limit(struct limit_list* list_of) {
	int effecting_limit = LIMIT_TOTALLY_RELEASED;
	struct limit_voter* iter;

	list_for_each_entry(iter, &list_of->head, node) {
		if (iter->limit < effecting_limit) {
			effecting_limit = iter->limit;
		}
	}

	return effecting_limit;
}

static int atomic_voter_id(void) {
	static int voter_id = 0;
	return voter_id++;
}

static void atomic_limit_to_veneer(struct limit_list* list_of) {
	int effecting_limit = atomic_effecting_limit(list_of);
	signal_to_veneer(list_of->type, effecting_limit);

	/* Notify to effecting voter here. In fact "->effected()" is not important to the voter driver.
	 * So please consider it when you do refactoring further.
	 */
	if (effecting_limit != LIMIT_TOTALLY_RELEASED) {
		struct limit_voter* iter;

		list_for_each_entry(iter, &list_of->head, node)	{
			if (iter->limit == effecting_limit && iter->effected) {
				iter->effected(iter);
			}
		}
	}
}

static void signal_to_voters(struct limit_list* container,
		enum limit_action action) {
	struct limit_voter* iter;

	list_for_each_entry(iter, &container->head, node) {
		if (action == ACTION_ACTIVE)
			iter->activated(iter);
		else
			iter->deactivated(iter);
	}
}

static void signal_to_veneer(enum limit_type type, int limit) {
	struct power_supply* psy_veneer = power_supply_get_by_name(
			"battery-veneer");

	if (psy_veneer && psy_veneer->set_property) {
		union power_supply_propval value = vote_make(type, limit);
		psy_veneer->set_property(psy_veneer,
			POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &value);
	}
}

struct limit_voter* limit_voter_getbyname(char* name) {
	int i;
	for (i = 0; i < ARRAY_SIZE(voter_list_total); ++i) {
		struct limit_voter* ret = limit_voter_get_by_name(
				voter_list_total[i], name);
		if (!ret)
			return ret;
	}

	return NULL;
}

int limit_voter_register(struct limit_voter* entry, const char* name,
		enum limit_type type,
		int (*activated)(struct limit_voter *entry),
		int (*effected)(struct limit_voter *entry),
		int (*deactivated)(struct limit_voter *entry)) {

	struct limit_list* list_to = limit_list_get_by_type(type);

	strlcpy(entry->name, name, LIMIT_NAME_LENGTH);
	entry->type = type;
	entry->limit = LIMIT_TOTALLY_RELEASED;

	entry->activated = activated;
	entry->effected = effected;
	entry->deactivated = deactivated;

	mutex_lock(&list_to->lock);
	entry->id = atomic_voter_id();
	list_add(&entry->node, &list_to->head);
	mutex_unlock(&list_to->lock);

	// TODO : need to check name duplication
	return 0;
}

void limit_voter_unregister(struct limit_voter* entry) {
	struct limit_list* list_from = limit_list_get_by_entry(entry);

	mutex_lock(&list_from->lock);
	list_del(&entry->node);
	mutex_unlock(&list_from->lock);
}

void limit_voter_activate() {
	int i;

	for (i=0; i<ARRAY_SIZE(voter_list_total); ++i) {
		struct limit_list* list_iter = voter_list_total[i];
		signal_to_voters(list_iter, ACTION_ACTIVE);

		mutex_lock(&list_iter->lock);
		atomic_limit_to_veneer(list_iter);
		mutex_unlock(&list_iter->lock);
	}
}

void limit_voter_deactivate() {
	int i;

	for (i=0; i<ARRAY_SIZE(voter_list_total); ++i) {
		struct limit_list* list_iter = voter_list_total[i];
		signal_to_voters(list_iter, ACTION_DEACTIVE);
		signal_to_veneer(list_iter->type, LIMIT_TOTALLY_RELEASED);
	}
}

void limit_voter_set(struct limit_voter* voter, int limit) {
	struct limit_list* list_of;

	if (!voter) {
		pr_voter("voter is NULL\n");
		return;
	}

	if (voter->limit == limit) {
		pr_voter("voting values are same, %d\n", limit);
		return;
	}

	list_of = limit_list_get_by_entry(voter);
	if (list_of) {
		voter->limit = limit;

		mutex_lock(&list_of->lock);
		atomic_limit_to_veneer(list_of);
		mutex_unlock(&list_of->lock);
	}
	else
		pr_voter("Couldn't find the list of %s\n", voter->name);
}

void limit_voter_release(struct limit_voter* voter) {
	limit_voter_set(voter, LIMIT_TOTALLY_RELEASED);
}

