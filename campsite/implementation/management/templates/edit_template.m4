INCLUDE_PHP_LIB(<*$ADMIN_DIR/templates*>)dnl
B_DATABASE

CHECK_BASIC_ACCESS
<?php if ($access == 0) { ?>
	X_LOGOUT
<?php } ?>
CHECK_ACCESS(<*ManageTempl*>)dnl

B_HEAD
<?php if ($access == 0) { ?>
	X_AD(<*You do not have the right to modify templates.*>)
<?php } ?>dnl
E_HEAD

<?php
if ($access) {
	SET_ACCESS(<*mta*>, <*ManageTempl*>)
	SET_ACCESS(<*dta*>, <*DeleteTempl*>)
	todef('Path');
	todef('Name');
?>

B_CURRENT
X_CURRENT(<*Path*>, <*<B><A HREF="/<?php echo $ADMIN; ?>/templates?Path=<?php pencHTML(decURL($Path)); ?>"><?php  pencHTML(decURL($Path)); ?></A></B>*>)
X_CURRENT(<*Template*>, <*<B><?php  pencHTML(decURL($Name)); ?></B>*>)
E_CURRENT

<P>
B_DIALOG(<*Edit template*>, <*POST*>, <*do_edit.php*>)
	B_DIALOG_BUTTONS
<?php  if ($dta != 0) { ?>
	SUBMIT(<*Save*>, <*Save changes*>)
	REDIRECT(<*Cancel*>, <*Cancel*>, <*<?php echo "/$ADMIN/templates?Path=".encHTML(decS($Path)); ?>*>)
<?php  } else { ?>
	REDIRECT(<*Done*>, <*Done*>, <*<?php echo "/$ADMIN/templates?Path=".encHTML(decS($Path)); ?>*>)
<?php  } ?>
	E_DIALOG_BUTTONS

<?php 
	$filename = $Campsite['HTML_DIR']."/look/".decURL($Path)."/$Name";
	if (filesize($filename) > 0) {
		$fd = fopen ($filename, "r");
		$contents = fread ($fd, filesize ($filename));
		fclose ($fd);
	} else {
		$contents = "";
	}
?>

	<TR><TD><TEXTAREA ROWS="25" COLS="85" NAME="cField" WRAP="NO"><?php  p(decS($contents)) ?></TEXTAREA></TD></TR>
	<INPUT TYPE="HIDDEN" NAME="Path" VALUE="<?php  p($Path); ?>">
	<INPUT TYPE="HIDDEN" NAME="Name" VALUE="<?php  p($Name); ?>">

	B_DIALOG_BUTTONS
<?php  if ($dta != 0) { ?>
	SUBMIT(<*Save*>, <*Save changes*>)
	REDIRECT(<*Cancel*>, <*Cancel*>, <*<?php echo "/$ADMIN/templates?Path=".encHTML(decS($Path)); ?>*>)
<?php  } else { ?>
	REDIRECT(<*Done*>, <*Done*>, <*<?php echo "/$ADMIN/templates?Path=".encHTML(decS($Path)); ?>*>)
<?php  } ?>
	E_DIALOG_BUTTONS
E_DIALOG

X_HR
X_COPYRIGHT
E_BODY
<?php  } ?>dnl

E_DATABASE
E_HTML
