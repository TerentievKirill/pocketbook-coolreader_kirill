# Website moved

The public website and hosting configuration now live in
[pocketbook-web](https://github.com/dderugin/pocketbook-web).
The live site is https://readertools.eu/. Existing download and app-update URLs remain available for installed readers.
Update website pages and release metadata in the standalone repository.

GitHub Pages publishes this directory from `master`. Keep the former homepage,
`installation.html` and `easy-connect.html` available as move notices. Generate
them with `python3 tools/generate-pages-redirects.py` after changing their targets.
Each has an immediate refresh and a matching canonical URL on readertools.eu;
bookmarked sections route to the corresponding new guide. The visible links work
without JavaScript. `.nojekyll` keeps these files unchanged during publication.

Search results should consolidate on ReaderTools. Do not publish a second copy
of the site here or add `noindex` to the move notices. The canonical site's
sitemap is https://readertools.eu/sitemap_index.xml.
