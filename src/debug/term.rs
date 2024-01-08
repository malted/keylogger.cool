use crossterm::{
    event::{self, Event, KeyCode},
    terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
    ExecutableCommand,
};
use ratatui::{prelude::*, widgets::*};
use std::io::{self, stdout};

pub fn start_term() -> io::Result<()> {
    enable_raw_mode()?;
    stdout().execute(EnterAlternateScreen)?;
    let mut terminal = Terminal::new(CrosstermBackend::new(stdout()))?;

    let mut should_quit = false;
    while !should_quit {
        terminal.draw(ui)?;
        should_quit = handle_events()?;
    }

    disable_raw_mode()?;
    stdout().execute(LeaveAlternateScreen)?;
    Ok(())
}

fn handle_events() -> io::Result<bool> {
    if event::poll(std::time::Duration::from_millis(50))? {
        if let Event::Key(key) = event::read()? {
            if key.kind == event::KeyEventKind::Press && key.code == KeyCode::Char('q') {
                return Ok(true);
            }
        }
    }
    Ok(false)
}

use std::collections::VecDeque;
static mut LAST_100_MOUSE_SPEEDS: VecDeque<f32> = VecDeque::new();
static mut LAST_100_SCROLL_SPEEDS: VecDeque<f32> = VecDeque::new();
static mut LAST_100_KEYBOARD_SPEEDS: VecDeque<f32> = VecDeque::new();

fn ui(frame: &mut Frame) {
    unsafe {
        if let Some(mouse_speed) = super::MOUSE_SPEED {
            LAST_100_MOUSE_SPEEDS.push_front(mouse_speed);

            if LAST_100_MOUSE_SPEEDS.len() > 100 {
                LAST_100_MOUSE_SPEEDS.pop_back();
            }
        }

        if let Some(scroll_speed) = super::SCROLL_SPEED {
            LAST_100_SCROLL_SPEEDS.push_front(scroll_speed);

            if LAST_100_SCROLL_SPEEDS.len() > 100 {
                LAST_100_SCROLL_SPEEDS.pop_back();
            }
        }

        if let Some(key_time_down_ms) = super::KEY_TIME_DOWN_MS {
            LAST_100_KEYBOARD_SPEEDS.push_front(key_time_down_ms as f32);

            if LAST_100_KEYBOARD_SPEEDS.len() > 100 {
                LAST_100_KEYBOARD_SPEEDS.pop_back();
            }
        }
    };

    let main_layout = Layout::new(
        Direction::Vertical,
        [
            Constraint::Length(1),
            Constraint::Length(3),
            Constraint::Min(0),
            Constraint::Length(1),
        ],
    )
    .split(frame.size());
    frame.render_widget(
        Block::new().borders(Borders::TOP).title("Title Bar"),
        main_layout[0],
    );

    unsafe {
        if let Some(last_exe_time_us) = super::LAST_EXECUTION_TIME_US {
            let status_bar_text = format!("Last execution Time: {} us", last_exe_time_us);
            let status_bar = Paragraph::new(Text::from(status_bar_text))
                .block(Block::new().borders(Borders::TOP).title("Status Bar"));

            frame.render_widget(status_bar, main_layout[3]);
        }
    }

    unsafe {
        if let Some(last_exe_time_us) = super::LAST_EXECUTION_TIME_US {
            frame.render_widget(
                Block::new().borders(Borders::TOP).title(format!(
                    "Metadata | Last event handler execution Time: {} µs ",
                    last_exe_time_us
                )),
                main_layout[3],
            );
        }
    }

    let inner_layout = Layout::new(
        Direction::Horizontal,
        [
            Constraint::Percentage(100 / 3),
            Constraint::Percentage(100 / 3),
            Constraint::Percentage(100 / 3),
        ],
    )
    .split(main_layout[1]);

    unsafe {
        render_mouse_movement(frame, inner_layout[0]);
        render_scroll(frame, inner_layout[1]);
        render_keystroke(frame, inner_layout[2]);
        render_speed_charts(frame, main_layout[2]);
    };

    frame.render_widget(
        Block::default().borders(Borders::ALL).title("Scrolling"),
        inner_layout[1],
    );
}

unsafe fn render_mouse_movement(frame: &mut Frame, layout: Rect) {
    let mouse_movement_block = Block::default()
        .title("Mouse Movement")
        .borders(Borders::ALL);

    if let (Some(dir), Some(speed)) = (super::MOUSE_DIR_EMOJI, super::MOUSE_SPEED) {
        let max_speed = LAST_100_MOUSE_SPEEDS
            .iter()
            .fold(0.0_f32, |acc: f32, x: &f32| acc.max(*x));

        let text = format!("{dir} {speed:>5.1}km/h   Max (last 100): {max_speed:>5.1}km/h");
        let paragraph = Paragraph::new(Text::from(text)).block(mouse_movement_block);

        frame.render_widget(paragraph, layout);
    }
}

unsafe fn render_scroll(frame: &mut Frame, layout: Rect) {
    let scroll_block = Block::default().title("Scrolling").borders(Borders::ALL);

    if let (Some(dir), Some(speed)) = (super::SCROLL_DIR_EMOJI, super::SCROLL_SPEED) {
        let max_speed = LAST_100_SCROLL_SPEEDS
            .iter()
            .fold(0.0_f32, |acc: f32, x: &f32| acc.max(*x));

        let text = format!("{dir} {speed:>5.1}km/h   Max (last 100): {max_speed:>5.1}km/h");
        let paragraph = Paragraph::new(Text::from(text)).block(scroll_block);

        frame.render_widget(paragraph, layout);
    }
}

unsafe fn render_keystroke(frame: &mut Frame, layout: Rect) {
    let scroll_block = Block::default().title("Keystrokes").borders(Borders::ALL);

    if let (Some(char), Some(time_down)) = (super::KEY_CHAR.clone(), super::KEY_TIME_DOWN_MS) {
        let min_down = LAST_100_KEYBOARD_SPEEDS
            .iter()
            .fold(f32::INFINITY, |acc: f32, x: &f32| acc.min(*x));

        let max_down = LAST_100_KEYBOARD_SPEEDS
            .iter()
            .fold(0.0_f32, |acc: f32, x: &f32| acc.max(*x));

        let text =
            format!("{char} {time_down}ms  min(..100):{min_down}ms  max(..100):{max_down}ms");
        let paragraph = Paragraph::new(Text::from(text)).block(scroll_block);

        frame.render_widget(paragraph, layout);
    }
}

fn render_speed_charts(f: &mut Frame, area: Rect) {
    unsafe {
        let mouse_speeds: Vec<(f64, f64)> = LAST_100_MOUSE_SPEEDS
            .iter()
            .enumerate()
            .map(|(i, speed)| (i as f64, *speed as f64))
            .collect();

        let scroll_speeds: Vec<(f64, f64)> = LAST_100_SCROLL_SPEEDS
            .iter()
            .enumerate()
            .map(|(i, speed)| (i as f64, *speed as f64))
            .collect();

        let x_labels = (0..=100)
            .step_by(20)
            .map(|x| Span::raw(format!("{}", x)))
            .collect();

        // Calculate the max_y bound based on the max values in both arrays
        let max_y = LAST_100_MOUSE_SPEEDS
            .iter()
            .chain(LAST_100_SCROLL_SPEEDS.iter())
            .fold(0.0_f32, |acc: f32, x: &f32| acc.max(*x));

        let datasets = vec![
            Dataset::default()
                .name("Mouse Speed")
                .marker(symbols::Marker::Dot)
                .style(Style::default().fg(Color::Cyan))
                .data(&mouse_speeds),
            Dataset::default()
                .name("Scroll Speed")
                .marker(symbols::Marker::Braille)
                .style(Style::default().fg(Color::Yellow))
                .data(&scroll_speeds),
        ];

        let chart = Chart::new(datasets)
            .block(Block::default().title("Speed Chart").borders(Borders::ALL))
            .x_axis(
                Axis::default()
                    .title("Events")
                    .style(Style::default().fg(Color::Gray))
                    .labels(x_labels)
                    .bounds([0.0, 100.0]),
            )
            .y_axis(
                Axis::default()
                    .title("Speed")
                    .style(Style::default().fg(Color::Gray))
                    .labels(vec![
                        Span::raw("0"),
                        Span::raw(format!("{:0.2}", max_y / 2.0)),
                        Span::raw(format!("{:0.2}", max_y)),
                    ])
                    .bounds([0.0, max_y.into()]),
            );

        f.render_widget(chart, area);
    }
}
