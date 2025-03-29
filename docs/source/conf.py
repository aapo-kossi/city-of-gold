# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'City of Gold'
copyright = '2025, Aapo Kössi'
author = 'Aapo Kössi'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.autosummary'
]

templates_path = ['_templates']
exclude_patterns = []
pygments_style = 'sphinx'


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

html_theme_options = {
    # 'analytics_id': 'G-XXXXXXXXXX',
    'analytics_anonymize_ip': True,
    'logo_only': False,
    'prev_next_buttons_location': 'bottom',
    'style_external_links': False,
    'vcs_pageview_mode': '',
    'flyout_display': 'hidden',
    'version_selector': True,
    'language_selector': True,
    # Toc options
    'sticky_navigation': True,
    'navigation_depth': 4,
}

# autodoc conf
autodoc_default_flags = ['members', 'undoc-members', 'show-inheritance']
exclude_patterns = []

# generate file stubs for submodules
autosummary_generate = ["vec/city_of_gold.vec.rst"]
autosummary_generate_overwrite = True

# doctest setup
import city_of_gold

