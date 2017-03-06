#include "objectbuilder.h"

#include <QDebug>
#include <QFileInfo>

ObjectBuilder::ObjectBuilder() {}

void ObjectBuilder::build()
{
	if(root["$type"].toString() == "object")
		generateApiObject();
	else if(root["$type"].toString() == "gadget")
		generateApiGadget();
	else
		throw QStringLiteral("REST_API_OBJECTS must be either of type object or gadget");
}

QString ObjectBuilder::specialPrefix()
{
	return "$";
}

void ObjectBuilder::generateApiObject()
{
	qInfo() << "generating object:" << className;

	auto includes = readIncludes();
	includes.append("QObject");
	includes.append("QString");
	readMembers();
	auto parent = root["$parent"].toString("QObject");

	//write header
	writeIncludes(header, includes);
	header << "class " << className << " : public " << parent << "\n"
		   << "{\n"
		   << "\tQ_OBJECT\n\n";
	writeProperties(true);
	header << "\npublic:\n"
		   << "\tQ_INVOKABLE " << className << "(QObject *parent = nullptr);\n\n";
	writeReadDeclarations();
	header << "\npublic Q_SLOTS:\n";
	writeWriteDeclarations();
	header << "\nQ_SIGNALS:\n";
	writeNotifyDeclarations();
	header << "\nprivate:\n";
	writeMemberDefinitions(header);
	header << "};\n\n";

	//write source
	source << "#include \"" << fileName << ".h\"\n\n"
		   << className << "::" << className << "(QObject *parent) :\n"
		   << "\t" << parent << "(parent)\n"
		   << "{}\n";
	writeReadDefinitions(false);
	writeWriteDefinitions(false);
}

void ObjectBuilder::generateApiGadget()
{
	qInfo() << "generating gadget:" << className;

	auto includes = readIncludes();
	includes.append("QSharedDataPointer");
	includes.append("QString");
	readMembers();
	auto parent = root["$parent"].toString();

	//write header
	writeIncludes(header, includes);
	header << "class " << className << "Data;\n";
	if(parent.isEmpty())
		header << "class " << className << "\n";
	else
		header << "class " << className << " : public " << parent << "\n";
	header << "{\n"
		   << "\tQ_GADGET\n\n";
	writeProperties(false);
	header << "\npublic:\n"
		   << "\t" << className << "();\n"
		   << "\t" << className << "(const " << className << " &other);\n"
		   << "\t~" << className << "();\n\n";
	writeReadDeclarations();
	header << "\n";
	writeWriteDeclarations();
	header << "\nprivate:\n"
		   << "\t QSharedDataPointer<" << className << "Data> d;\n"
		   << "};\n\n";

	//write source
	source << "#include \"" << fileName << ".h\"\n\n";
	writeDataClass();
	source << className << "::" << className << "() :\n";
	if(!parent.isEmpty())
		source << "\t" << parent << "(),\n";
	source << "\td(new " << className << "Data())\n"
		   << "{}\n\n"
		   << className << "::" << className << "(const " << className << " &other) :\n";
	if(!parent.isEmpty())
		source << "\t" << parent << "(other),\n";
	source << "\td(other.d)\n"
		   << "{}\n\n"
		   << className << "::~" << className << "() {}\n";
	writeReadDefinitions(true);
	writeWriteDefinitions(true);
}

void ObjectBuilder::readMembers()
{
	for(auto it = root.constBegin(); it != root.constEnd(); it++) {
		if(it.key().startsWith("$"))
			continue;
		members.insert(it.key(), it.value().toString());
	}
}

QString ObjectBuilder::setter(const QString &name)
{
	QString setterName = "set" + name;
	setterName[3] = setterName[3].toUpper();
	return setterName;
}

void ObjectBuilder::writeProperties(bool withNotify)
{
	for(auto it = members.constBegin(); it != members.constEnd(); it++) {
		header << "\tQ_PROPERTY(" << it.value() << " " << it.key()
			   << " READ " << it.key()
			   << " WRITE " << setter(it.key());
		if(withNotify)
			header << " NOTIFY " << it.key() << "Changed";
		header << ")\n";
	}
}

void ObjectBuilder::writeReadDeclarations()
{
	for(auto it = members.constBegin(); it != members.constEnd(); it++)
		header << "\t" << it.value() << " " << it.key() << "() const;\n";
}

void ObjectBuilder::writeWriteDeclarations()
{
	for(auto it = members.constBegin(); it != members.constEnd(); it++)
		header << "\tvoid " << setter(it.key()) << "(" << it.value() << " " << it.key() << ");\n";
}

void ObjectBuilder::writeNotifyDeclarations()
{
	for(auto it = members.constBegin(); it != members.constEnd(); it++)
		header << "\tvoid " << it.key() << "Changed(" << it.value() << " " << it.key() << ");\n";
}

void ObjectBuilder::writeMemberDefinitions(QTextStream &stream)
{
	for(auto it = members.constBegin(); it != members.constEnd(); it++)
		stream << "\t" << it.value() << " _" << it.key() << ";\n";
}

void ObjectBuilder::writeReadDefinitions(bool asGadget)
{
	auto prefix = asGadget ? QStringLiteral("d->_") : QStringLiteral("_");
	for(auto it = members.constBegin(); it != members.constEnd(); it++) {
		source << "\n" << it.value() << " " << className << "::" << it.key() << "() const\n"
			   << "{\n"
			   << "\treturn " << prefix << it.key() << ";\n"
			   << "}\n";
	}
}

void ObjectBuilder::writeWriteDefinitions(bool asGadget)
{
	auto prefix = asGadget ? QStringLiteral("d->_") : QStringLiteral("_");
	for(auto it = members.constBegin(); it != members.constEnd(); it++) {
		source << "\nvoid " << className << "::" << setter(it.key()) << "(" << it.value() << " " << it.key() << ")\n"
			   << "{\n"
			   << "\tif(" << prefix << it.key() << " == " << it.key() << ")\n"
			   << "\t\treturn;\n\n"
			   << "\t" << prefix << it.key() << " = " << it.key() <<";\n";
		if(!asGadget)
			source << "\temit " << it.key() << "Changed(" << it.key() << ");\n";
		source << "}\n";
	}
}

void ObjectBuilder::writeDataClass()
{
	auto name = className + "Data";
	source << "class " << name << " : public QSharedData\n"
		   << "{\n"
		   << "public:\n"
		   << "\t" << name << "();\n"
		   << "\t" << name << "(const " << name << " &other);\n\n";
	writeMemberDefinitions(source);
	source << "};\n\n"
		   << name << "::" << name << "() :\n"
		   << "\tQSharedData()\n"
		   << "{}\n\n"
		   << name << "::" << name << "(const " << name << " &other) :\n"
		   << "\tQSharedData(other)\n";
	writeMemberCopyDefinitions(source);
	source << "{}\n\n";
}

void ObjectBuilder::writeMemberCopyDefinitions(QTextStream &stream)
{
	for(auto it = members.constBegin(); it != members.constEnd(); it++)
		stream << "\t,_" << it.key() << "(other._" << it.key() << ")\n";
}
