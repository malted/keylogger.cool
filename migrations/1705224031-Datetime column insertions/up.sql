ALTER TABLE keyboard_event    ADD COLUMN datetime INTEGER NOT NULL;
ALTER TABLE mouse_click_event ADD COLUMN datetime INTEGER NOT NULL;
ALTER TABLE mouse_move_event  ADD COLUMN datetime INTEGER NOT NULL;
ALTER TABLE scroll_event      ADD COLUMN datetime INTEGER NOT NULL;
