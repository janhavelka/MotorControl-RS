# Searchable PDF extracts

These are raw PyMuPDF text extracts of the downloaded originals. Headings use
physical PDF page numbers starting at 1, which can differ from the manual's
printed page numbers. Tables, diagrams, superscripts, and symbols may be
misordered or absent. Always verify ambiguous content in the original PDF.

- [Motor Modbus function manual](Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.md)
- [ESS23-RS hardware manual](ESS23-RS1020_Series_Bus_Integrated_Motor_Hardware_Manual.md)
- [RS20 datasheet](ESS23-RS20_Full_Datasheet.md)
- [RS10 datasheet](ESS23-RS10_Full_Datasheet.md)
- [Modbus application protocol](Modbus_Application_Protocol_V1.1b3.md)
- [Modbus serial-line specification](Modbus_Serial_Line_V1.02.md)

Regenerate using `python scripts/prepare_references.py` from the project root.

Lichuan LC57BK-R / RC research sources have their own preserved manifest and
extract command, `python scripts/prepare_lichuan_references.py --check --extract`:

- [LC57BK-R user manual, V1.1 filename, Chinese](Lichuan_LC57BK-R_User_Manual_V1.1_CN.md)
- [LC57BK-R operation manual, Chinese](Lichuan_LC57BK-R_Operation_Manual_CN.md)

See the [English engineering review](../reference/13_lichuan_lc57bk_rs485.md)
for conflicting addresses, labels and encoder claims. Extraction does not resolve them.

The separate Lichuan TL57-R L-type family uses
`python scripts/prepare_lichuan_tl57_references.py --check --extract`:

- [TL57-R user manual, Chinese](Lichuan_TL57-R_User_Manual_CN.md)
- [TL57-R operation manual, Chinese](Lichuan_TL57-R_Operation_Manual_CN.md)
- [TL-R serial examples, Chinese](Lichuan_TL-R_Serial_Examples_CN.md)

See its [English engineering review](../reference/14_lichuan_tl57_rs485.md)
for the independent register map, malformed examples and unresolved settings.
