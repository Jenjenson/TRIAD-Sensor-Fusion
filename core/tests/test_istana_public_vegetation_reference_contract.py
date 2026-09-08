"""Offline contract checks for the public, non-geographic vegetation reference."""

from __future__ import annotations

import re
import unittest
from pathlib import Path
from urllib.parse import urlsplit


REPO = Path(__file__).absolute().parents[2]
REFERENCE = REPO / "docs" / "ISTANA_PUBLIC_VEGETATION_REFERENCE.md"

EXPECTED_SCHEMA = "triad.istana.public_vegetation_reference.v1"
EXPECTED_STATUS = "PUBLIC_NON_GEOGRAPHIC_VISUAL_REFERENCE"
EXPECTED_LINKS = {
    "S1": "https://www.istana.gov.sg/visit-and-explore/the-grounds/",
    "S2": "https://www.istana.gov.sg/presidents-office/meet-our-people/",
    "S3": "https://www.roots.gov.sg/stories-landing/stories/iconic-trees-in-singapores-civic-district",
    "S4": "https://heritagetrees.nparks.gov.sg/heritagetrees/ht-2003-108/",
    "S4a": "https://www.nparks.gov.sg/florafaunaweb/flora/3/1/3106",
    "S5": "https://heritagetrees.nparks.gov.sg/heritagetrees/ht-2003-87/",
    "S6": "https://www.istana.gov.sg/newsroom/news-release-detail-page/",
    "S7": "https://www.weather.gov.sg/climate-climate-of-singapore/",
    "S8": "https://www.roots.gov.sg/places/places-landing/Places/national-monuments/the-istana-and-sri-temasek",
    "S9": "https://www.istana.gov.sg/terms-of-use/",
    "S10": "https://www.istana.gov.sg/sitemap.xml",
}
ALLOWED_HOSTS = {
    "heritagetrees.nparks.gov.sg",
    "www.istana.gov.sg",
    "www.nparks.gov.sg",
    "www.roots.gov.sg",
    "www.weather.gov.sg",
}
EXPECTED_HEADINGS = (
    "## Purpose and safe-use boundary",
    "## Publicly supported landscape character",
    "## Species-bound public evidence",
    "## Visual-form mapping to project proxies",
    "## Seasonal and colour cues",
    "## Concrete non-geographic modeling targets",
    "## Acceptance checklist",
    "## Source ledger and limitations",
)
EXPECTED_PROXIES = ("umbrella", "dome", "high-fork", "columnar", "palm")
EXPECTED_TAXA = (
    "Samanea saman",
    "Mangifera caesia",
    "Dacrydium elatum",
)


class IstanaPublicVegetationReferenceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.raw = REFERENCE.read_bytes()
        cls.text = cls.raw.decode("utf-8")

    def test_canonical_text_and_schema(self) -> None:
        self.assertNotIn(b"\r", self.raw)
        self.assertTrue(self.raw.endswith(b"\n"))
        self.assertIn(f"Reference schema: `{EXPECTED_SCHEMA}`", self.text)
        self.assertIn(f"Status: `{EXPECTED_STATUS}`", self.text)
        self.assertIn("Evidence accessed: `2026-09-07` (Asia/Singapore)", self.text)

        positions = [self.text.index(heading) for heading in EXPECTED_HEADINGS]
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(len(re.findall(r"^## ", self.text, flags=re.MULTILINE)), len(EXPECTED_HEADINGS))

    def test_exact_authoritative_https_source_roster(self) -> None:
        definitions = dict(
            re.findall(r"^\[(S\d+a?)\]: (https://\S+)$", self.text, flags=re.MULTILINE)
        )
        self.assertEqual(definitions, EXPECTED_LINKS)

        for source_id, url in definitions.items():
            parsed = urlsplit(url)
            self.assertEqual(parsed.scheme, "https", source_id)
            self.assertIn(parsed.hostname, ALLOWED_HOSTS, source_id)
            self.assertFalse(parsed.username, source_id)
            self.assertFalse(parsed.password, source_id)
            self.assertFalse(parsed.query, source_id)
            self.assertFalse(parsed.fragment, source_id)

    def test_proxy_roster_species_and_claim_boundaries(self) -> None:
        for proxy in EXPECTED_PROXIES:
            self.assertEqual(
                len(re.findall(rf"^\| `{re.escape(proxy)}` \|", self.text, flags=re.MULTILINE)),
                1,
                proxy,
            )
        for taxon in EXPECTED_TAXA:
            self.assertIn(taxon, self.text)

        required_boundaries = (
            "not a current botanical inventory",
            "no precise tree coordinates",
            "no precise tree coordinates, security or access layout",
            "no world position",
            "not a current count",
            "not a species label",
            "embeds no source imagery",
            "Search snippets and third-party plant lists were not treated as evidence.",
        )
        lower_text = self.text.lower()
        for boundary in required_boundaries:
            self.assertIn(boundary.lower(), lower_text)

        self.assertNotIn("![", self.text)
        coordinate_pair = re.compile(
            r"(?<![\w.])[+-]?\d{1,3}\.\d{4,}\s*[,/]\s*[+-]?\d{1,3}\.\d{4,}(?![\w.])"
        )
        self.assertIsNone(coordinate_pair.search(self.text))

    def test_source_ledger_has_dates_confidence_and_limitations(self) -> None:
        for source_id in EXPECTED_LINKS:
            self.assertRegex(
                self.text,
                re.compile(rf"^\| {re.escape(source_id)} \|", flags=re.MULTILINE),
                source_id,
            )

        for expected_date in (
            "2025-11-27",
            "2026-07-03",
            "2021-08-26",
            "1991-2020",
            "2026-09-07",
        ):
            self.assertIn(expected_date, self.text)

        for confidence in ("**HIGH**", "**MEDIUM**", "**LOW**"):
            self.assertIn(confidence, self.text)
        self.assertIn("## Source ledger and limitations", self.text)
        self.assertIn("SLA/OneMap was not needed", self.text)
        self.assertIn(
            "The current official sitemap exposes this migrated article only at the generic route; "
            "no article-specific URI can be stabilized as of the access date",
            self.text,
        )


if __name__ == "__main__":
    unittest.main()
