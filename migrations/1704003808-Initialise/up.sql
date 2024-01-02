CREATE TABLE process (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	name TEXT UNIQUE
);

CREATE TABLE keyboard_event (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	type INTEGER,
	is_builtin_display INTEGER,
	is_main_display INTEGER,
	process_id INTEGER,
	execution_time_us INTEGER,

	time_down_ms INTEGER,
	key_code INTEGER,
	key_char TEXT,

	FOREIGN KEY(process_id) REFERENCES process(id)
);

CREATE TABLE mouse_event (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	type INTEGER,
	is_builtin_display INTEGER,
	is_main_display INTEGER,
	process_id INTEGER,
	execution_time_us INTEGER,

	time_down_ms INTEGER,
	normalised_x REAL,
	normalised_y REAL,
	dragged_distance_px REAL,
	dragged_distance_mm REAL,

	FOREIGN KEY(process_id) REFERENCES process(id)
);

CREATE TABLE scroll_event (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	type INTEGER,
	is_builtin_display INTEGER,
	is_main_display INTEGER,
	process_id INTEGER,
	execution_time_us INTEGER,

	scroll_delta_axis_1 INTEGER,
	scroll_delta_axis_2 INTEGER,

	FOREIGN KEY(process_id) REFERENCES process(id)
);

CREATE TABLE migration (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	direction CHAR(1) CHECK(direction in ('U', 'D')) NOT NULL,
	migration_version_timestamp INTEGER NOT NULL
);
