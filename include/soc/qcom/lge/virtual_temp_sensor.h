struct value_sensor {
	const char			*name;
	int				vts_index;
	void				*devdata;
	int				weight;
	int				(*get_temp)(void *data);
	struct list_head		list;
};

int vts_register_value_sensor(struct value_sensor *vs);
void vts_unregister_value_sensor(struct value_sensor *vs);
