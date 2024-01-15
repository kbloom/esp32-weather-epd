package main

import (
	"github.com/0xazure/gofpdf"
)

func main() {
	pdf := gofpdf.New("L", "mm", "Letter", "")

	const (
		originX            = 15
		originY            = 15
		frameWidth         = 194
		frameHeight        = 143
		displayWidth       = 170.2
		displayHeight      = 111.2
		usableWidth        = 165.24
		usableHeight       = 99.74
		topMargin          = 2.47
		textVerticalOffset = 3
	)

	pdf.AddPage()

	pdf.SetDrawColor(0, 0, 0)
	pdf.SetFont("Helvetica", "", 9)

	pdf.Rect(originX, originY, frameWidth, frameHeight, "D")
	pdf.Text(originX+1, originY+textVerticalOffset, "Bounding box for the frame")

	const (
		usableX = originX + (frameWidth-usableWidth)/2
		usableY = originY + (frameHeight-usableHeight)/2
	)

	pdf.Rect(usableX, usableY, usableWidth, usableHeight, "D")
	pdf.Text(usableX+1, usableY+textVerticalOffset, "Cut out this box for viewport")

	const (
		displayX = originX + (frameWidth-displayWidth)/2
		displayY = usableY - topMargin
	)

	pdf.Text(displayX+1, displayY-textVerticalOffset, "Mark corners of this box on back of paper, align corners of e-paper display to these marks")
	pdf.Rect(displayX, displayY, displayWidth, displayHeight, "D")

	pdf.Text(usableX+20, usableY+20, "Ensure that this PDF is printed in landscape mode, and at original size")
	pdf.OutputFileAndClose("diagram.pdf")
}
