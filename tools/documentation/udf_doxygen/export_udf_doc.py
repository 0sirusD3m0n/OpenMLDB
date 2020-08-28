import os
import subprocess
import yaml

DOXYGEN_DIR = os.path.abspath(os.path.dirname(__file__))
HOME_DIR = os.path.join(DOXYGEN_DIR, "../../..")
BUILD_DIR = os.path.abspath(os.path.join(HOME_DIR, "build"))
TMP_DIR = os.path.join(BUILD_DIR, "docs/tmp")


def export_yaml():
	if not os.path.exists(TMP_DIR):
		os.makedirs(TMP_DIR)
	ret = subprocess.call(
		[os.path.join(BUILD_DIR, "src/export_udf_info"),
		 "--output_dir", TMP_DIR,
		 "--output_file", "udf_defs.yaml"])
	if ret != 0:
		print("Invoke native export udf binary failed")
		exit(ret)


def process_doc(doc):
	lines = doc.split("\n")
	first_line_indent = -1;
	for i in range(0, len(lines)):
		line = lines[i]
		if line.strip() == "":
			continue
		if first_line_indent < 0:
			first_line_indent = len(line) - len(line.lstrip())
		indent = min(len(line) - len(line.lstrip()), first_line_indent)
		lines[i] = lines[i][indent: ]
	return "\n".join(lines)


def merge_arith_types(signature_set):
	arith_types = frozenset(["int16", "int32", "int64", "float", "double"])
	arith_list_types = frozenset(["list_int16", "list_int32", "list_int64", "list_float", "list_double"])
	found = True

	def __find_and_merge(arg_types, idx, list_ty, merge_ty):
		merge_keys = []
		if arg_types[idx] in list_ty:
			# print("check for " + ", ".join(arg_types))
			for dtype in list_ty:
				origin = arg_types[idx]
				arg_types[idx] = dtype
				cur_key = ", ".join(arg_types)
				arg_types[idx] = origin
				merge_keys.append(cur_key)
				if not cur_key in signature_set:
					# print(cur_key + " not defined: " + str(idx))
					break
			else:
				for key in merge_keys:
					signature_set.pop(key)
				arg_types[idx] = merge_ty
				new_key = ", ".join(arg_types)
				print(new_key, signature_set)
				signature_set[new_key] = arg_types
				return True
		return False

	while found:
		found = False
		for key in signature_set:
			arg_types = [_ for _ in signature_set[key]]
			for i in range(len(arg_types)):
				if __find_and_merge(arg_types, i, arith_types, "number"):
					found = True
					break
				elif __find_and_merge(arg_types, i, arith_list_types, "list_number"):
					found = True
					break
			if found: break

	return signature_set


def make_header():
	with open(os.path.join(TMP_DIR, "udf_defs.yaml")) as yaml_file:
		udf_defs = yaml.load(yaml_file.read())
	
	if not os.path.exists(DOXYGEN_DIR + "/udfs"):
		os.makedirs(DOXYGEN_DIR + "/udfs")
	fake_header = os.path.join(DOXYGEN_DIR + "/udfs/udfs.h")

	with open(fake_header, "w") as header_file:
		for name in sorted(udf_defs.keys()):
			content = "/**\n"

			content += "@par Description\n"
			items = udf_defs[name]
			# print("Found %d registries for \"%s\"" % (len(items), name))
			for item in items:
				doc = item["doc"]
				if doc.strip() != "":
					content += process_doc(doc)
					break;
			content += "\n\n@par Supported Types\n"
			sig_set = dict()
			sig_list = []
			for item in items:
				is_variadic = item["is_variadic"]
				for sig in item["signatures"]:
					arg_types = sig["arg_types"]
					if is_variadic:
						arg_types.append("...")
					return_type = sig["return_type"]
					key = ", ".join(arg_types)
					if key not in sig_set:
						sig_set[key] = arg_types
			
			# merge for number type
			sig_set = merge_arith_types(sig_set)

			sig_list = sorted([_ for _ in sig_set])
			for sig in sig_list:
				content += "- [" + sig + "]\n"

			content += "\n*/\n"
			content += name + "();\n"
			header_file.write(content)


def doxygen():
	ret = subprocess.call(["doxygen"], cwd=DOXYGEN_DIR)
	if ret != 0:
		print("Invoke doxygen failed")
		exit(ret)


if __name__ == "__main__":
	export_yaml()
	make_header()
	doxygen()
