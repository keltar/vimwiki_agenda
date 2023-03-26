# vimwiki_agenda
Agenda viewer for vimwiki tasks

Inspired by Emacs org-mode-agenda, this tool parses your active vimwikis for special time markers (see Time markers section) and reports notes that are past deadline, past scheduled date, or upcoming.

It uses `vim` to query `g:vimwiki_list` variable, and parses all wikis listed there.

### Time markers
To schedule note at some specific time, use `@1970-12-31 12:50` marker (time is optional).

To set a deadline, use `@!1970-12-31 12:50`.

### Usage

Example wiki file:
```
== my notes ==
 - [ ] check how vimwiki_agenda works @2000-01-01
 - [X] completed task will be excluded
```

```
./vimwiki_agenda
PAST SCHEDULED
2000-01-01 00:00:00 tasks.md
 - [ ] check how vimwiki_agenda works @2000-01-01
```

### Planned features
 - [ ] configurable filters
	 - [ ] filter wikis to include/exclude
	 - [ ] filter files to include/exclude
	 - [ ] filter note tags to include/exclude
 - [ ] configurable output
	 - [ ] coloured output should be disableable, both via configuration file and via command line option
	 - [ ] auto-disable colouring if not outputing to terminal (e.g. pipe to `less`)
 - [ ] one-way google calendar syncronisation
	- [ ] auto-add events to calendar
	- [ ] push changes to calendar (re-schedule, description change, etc.)
	- [ ] optionally filter by tags (only sync selected tags, or exclude tags from sync)
	- [ ] insert sync marker/calendar link to note comment
- [ ] one-way sync with any caldav/ical calendar
- [ ] support for other vimwiki syntax (currently only support 'default' vimwiki)
- [ ] vim integration
	- [ ] output agenda to split buffer
	- [ ] agenda elements should be links to position in files
- [ ] OSX support
- [ ] Windows support
